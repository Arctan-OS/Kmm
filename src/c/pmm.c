/**
 * @file pmm.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan-OS/Kernel - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan-OS/Kernel.
 *
 * Arctan is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * @DESCRIPTION
 * Initialize the physical memory manager.
 * On x86-64 system, low memory can be defined as all memory below 1 MiB. If the option
 * to reserve low memory is set at compile, this memory will not be used in the initialization
 * of the physical memory manager.
 *
 * The physical memory manager consists of two unique allocator types: freelist and buddy.
 * The first step is to segment memory greedily in page sized blocks. On x86-64 such blocks
 * would be of sizes and in order of: 1 GiB, 2 MiB, and 4 KiB. With this segmentation, freelists
 * can be constructed for each page size that is a power of two. Now, on x86-64, allocations
 * of 1 GiB, 2 MiB, and 4 KiB can be made in constant time provided that the freelists exist
 * for the desired size.
 *
 * Additionally, if sizes between the intervals of the page sizes is desired, then the buddy
 * allocators will answer. These buddy allocators will have the same largest sizes as the blocks (i.e.
 * on x86-64: 1 GiB, 2 MiB), and the smallest size of the smallest power of two page size (i.e. on
 * x86-64: 4 KiB). These buddy allocators may now be used for power two allocations between the
 * largest page size and the smallest page size.
 *
 * The order of operations is as follows:
 *  1. Check for a freelist of that has the same block size as the requested
 *     size
 *     1.1 If such a list is found, preform a freelist allocation and return.
 *         Otherwise proceed to 2
 *  2. Fit the requested size to the next largest power two page size, and
 *     find the first buddy allocator associated with this size (the buddy allocators are stored in a reverse linked list)
 *     2.1 If an allocation is made with this buddy allocator, return.
 *         Otherwise proceed to step 3
 *  3. Choose to:
 *      create a new buddy allocator (which can be done in constant time)
 *      look for an existing buddy allocator that can make the allocation (done in linear time)
 *      fail
 *
 * This introduced a plethora of challenges in determining the domain in which an allocation was
 * made. This is a problem as this is a memory manager, not just an allocator, so freeing must also
 * be preformed.
 *
 * Idea 0:
 *  Loop through all buddy allocators, and check their bounds to see if the allocated address falls
 *  within them. If it does: call a free for that address using the buddy allocator. Otherwise,
 *  loop through each freelist and check their bounds to see if the allocated address falls
 *  within them. If it does: call a free for that address using the freelist allocator. Otherwise,
 *  fail.
 *
 * Biasing Function: (USE AN ARRAY)
 *  A polynomial function, b, is defined to bias the memory manager towards certain powers of two, or
 *  page sizes. This is done by computing | b(x) |, where x is the index of the power of two from the starting
 *  power of two (x belongs to I). The larger the computed value, the lower the emphasis, the lower the computed
 *  value, the more emphasis. | b(x) | = 0, represents the most emphasis.
 *
 *  NOTE: For implementation simplicity, only when | b(x) | = 0 does the given power of two get emphasis placed on it.
 *        Functions for b should have a very large integer coefficient such as 1024x(x-1)(x-2)...
 *
 *  Emphasis is represented by a target value representing the number of segments of each power of two
 *  in the freelists. These powers of two are initialized before any other power of two to ensure that
 *  there are at least some of them.
 *
 *  This is because the freelist allocator will always be much faster than the buddy allocator.
*/
#include "arctan.h"
#include "config.h"
#include "lib/atomics.h"
#include <mm/algo/pbuddy.h>
#include <arch/info.h>
#include <mm/pmm.h>
#include <arch/x86-64/config.h>
#include <global.h>
#include <lib/util.h>
#include <mm/algo/pfreelist.h>
#include <mm/algo/vwatermark.h>
#include <stdint.h>

static uint32_t pmm_bias_count_high = sizeof(pmm_biases_high) / sizeof(struct ARC_PMMBiasConfigElement);
static uint32_t pmm_bias_count_low = sizeof(pmm_biases_low) / sizeof(struct ARC_PMMBiasConfigElement);

static uint32_t max_address_width = 0;
static struct ARC_VWatermark pmm_init_alloc = { 0 };
static struct ARC_VWatermarkMeta pmm_init_alloc_meta = { 0 };

static struct ARC_PFreelist *pmm_freelists_high = NULL;
static struct ARC_PBuddy *pmm_buddies_high = NULL;

static struct ARC_PFreelist *pmm_freelists_low = NULL;
static struct ARC_PBuddy *pmm_buddies_low = NULL;

// This is a pool of fast pages. When you need an O(1) allocation of a
// the PAGE_SIZE, use this by calling pmm_fast_page_alloc and USE pmm_fast_page_free.
// Fast pages is a non-contiguous freelist with an object size that of the smallest
// power two page size
struct ARC_PFreelistNode *fast_page_pool_high = NULL;
static size_t fast_page_count_high = 0;
static size_t fast_page_allocated_high = 0;

struct ARC_PFreelistNode *fast_page_pool_low = NULL;
static size_t fast_page_count_low = 0;
static size_t fast_page_allocated_low = 0;

static void *pmm_internal_alloc(size_t size, bool low) {
        const struct ARC_PMMBiasConfigElement *biases = low ? pmm_biases_low : pmm_biases_high;
        struct ARC_PFreelist *freelists = low ? pmm_freelists_low : pmm_freelists_high;
        struct ARC_PBuddy *buddies = low ? pmm_buddies_low : pmm_buddies_high;
        uint32_t pmm_bias_count = low ? pmm_bias_count_low : pmm_bias_count_high;

        SIZE_T_NEXT_POW2(size);
        uint32_t size_exponent = __builtin_ctz(size);

        if (size_exponent == PAGE_SIZE_LOWEST_EXPONENT) {
                return pmm_fast_page_alloc();
        }

        if (freelists[size_exponent].head != NULL) {
                return pfreelist_alloc(&freelists[size_exponent]);
        }

        uint32_t i = 0;
        uint32_t t_size_exp = UINT32_MAX;
        uint32_t buddy_min_exp = 0;
        for (; i < pmm_bias_count; i++) {
                if (biases[i].exp < t_size_exp && t_size_exp > size_exponent) {
                        t_size_exp = biases[i].exp;
                        buddy_min_exp = biases[i].min_buddy_exp;
                }
        }

        if (buddies[t_size_exp].head == NULL) {
                uintptr_t base = (uintptr_t)pfreelist_alloc(&freelists[t_size_exp]);

                if (base == 0) {
                        goto get_more_mem;
                }

                buddies[t_size_exp].exp = -1;

                if (init_pbuddy(&buddies[t_size_exp], base, t_size_exp, buddy_min_exp) != 0) {
                        goto get_more_mem;
                }
        }

        return pbuddy_alloc(&buddies[t_size_exp], size);

        get_more_mem:;
        ARC_DEBUG(ERR, "Failed to allocate %lu bytes of memory!\n", size);
        ARC_HANG; // TODO: Add code to reclaim unused bits of memory to attempt
                  //       fulfill allocation.

        return NULL;
}

static size_t pmm_internal_free(void *address, bool low) {
        const struct ARC_PMMBiasConfigElement *biases = low ? pmm_biases_low : pmm_biases_high;
        struct ARC_PFreelist *freelists = low ? pmm_freelists_low : pmm_freelists_high;
        struct ARC_PBuddy *buddies = low ? pmm_buddies_low : pmm_buddies_high;
        uint32_t pmm_bias_count = low ? pmm_bias_count_low : pmm_bias_count_high;

        if (address == NULL) {
                return 0;
        }

        for (uint32_t i = 0; i < pmm_bias_count; i++) {
                int bias = biases[i].exp;

                size_t s = pbuddy_free(&buddies[bias], address);
                if (s > 0) {
                        return s;
                }

                if (pfreelist_free(&freelists[bias], address) == address) {
                        return (1 << bias);
                }
        }

        pmm_fast_page_free(address);

        return PAGE_SIZE;
}

void *pmm_alloc(size_t size) {
        return pmm_internal_alloc(size, false);
}

size_t pmm_free(void *address) {
        return pmm_internal_free(address, false);
}

void *pmm_low_alloc(size_t size) {
        return pmm_internal_alloc(size, true);
}

size_t pmm_low_free(void *address) {
        return pmm_internal_free(address, true);
}

size_t pmm_alloc_fast_pages(size_t count, bool low) {
        (void)count;
        (void)low;
        return -1;
}

void *pmm_fast_page_alloc() {
        retry:;
        if (fast_page_count_high == fast_page_allocated_high && pmm_alloc_fast_pages(16, false) != 0) {
                return NULL;
        }

        struct ARC_PFreelistNode *node = NULL;

        if (fast_page_pool_high == NULL) {
              goto retry;
        }

        ARC_ATOMIC_XCHG(&fast_page_pool_high, &fast_page_pool_high->next, &node);
        ARC_ATOMIC_INC(fast_page_allocated_high);

        return (void *)node;
}

size_t pmm_fast_page_free(void *address) {
        if (address == NULL) {
                return 0;
        }

        struct ARC_PFreelistNode *entry = (struct ARC_PFreelistNode *)address;
        ARC_ATOMIC_XCHG(&fast_page_pool_high, &entry, &entry->next);
        ARC_ATOMIC_DEC(fast_page_allocated_high);

        return PAGE_SIZE;
}

void *pmm_fast_page_alloc_low() {
        retry:;
        if (fast_page_count_low == fast_page_allocated_low && pmm_alloc_fast_pages(16, true) != 0) {
                return NULL;
        }

        struct ARC_PFreelistNode *node = NULL;

        if (fast_page_pool_low == NULL) {
                goto retry;
        }

        ARC_ATOMIC_XCHG(&fast_page_pool_low, &fast_page_pool_low->next, &node);
        ARC_ATOMIC_INC(fast_page_allocated_low);

        return (void *)node;
}

size_t pmm_fast_page_free_low(void *address) {
        if (address == NULL || (uintptr_t)address >= ARC_PHYS_TO_HHDM(ARC_PMM_LOW_MEM_LIM)) {
                return 0;
        }

        struct ARC_PFreelistNode *entry = (struct ARC_PFreelistNode *)address;
        ARC_ATOMIC_XCHG(&fast_page_pool_low, &entry, &entry->next);
        ARC_ATOMIC_DEC(fast_page_allocated_low);

        return PAGE_SIZE;
}

static int pmm_create_freelists(struct ARC_MMap *mmap, int entries) {
        int initialized = 0;

        for (int i = 0; i < entries; i++) {
                struct ARC_MMap entry = mmap[i];

                if (entry.type != ARC_MEMORY_AVAILABLE) {
                        continue;
                }

                uintptr_t base = ARC_PHYS_TO_HHDM(entry.base);
                uintptr_t ceil = base + mmap[i].len;
                uintptr_t len = entry.len;

                const struct ARC_PMMBiasConfigElement *biases = pmm_biases_high;
                struct ARC_PFreelist *freelists = pmm_freelists_high;
                size_t *fast_page_count = &fast_page_count_high;
                struct ARC_PFreelistNode **fast_page_pool = &fast_page_pool_high;
                uint32_t pmm_bias_count = pmm_bias_count_high;

                ARC_DEBUG(INFO, "Entry %d (0x%"PRIx64" -> 0x%"PRIx64") suitable for initialization\n", i, base, ceil);

                if (entry.base < ARC_PMM_LOW_MEM_LIM) {
                        biases = pmm_biases_low;
                        freelists = pmm_freelists_low;
                        fast_page_count = &fast_page_count_low;
                        fast_page_pool = &fast_page_pool_low;
                        pmm_bias_count = pmm_bias_count_low;
                        ARC_DEBUG(INFO, "Entry is in low memory\n");
                } else {
                         ARC_DEBUG(INFO, "Entry is in high memory\n");
                }

                for (uint32_t j = 0; j < pmm_bias_count; j++) {
                        if (biases[j].ratio.numerator == 0) {
                                continue;
                        }

                        int bias = biases[j].exp;
                        size_t object_size = 1 << bias;

                        if (len < biases[j].min_blocks * object_size) {
                                continue;
                        }

                        // Initialize list
                        size_t range_len = ALIGN(len * biases[j].ratio.numerator / biases[j].ratio.denominator, object_size);
                        if (range_len > len) {
                                range_len = (len >> bias) << bias;
                        }

                        if (init_pfreelist(&freelists[bias], base, base + range_len, object_size) != 0) {
                                ARC_DEBUG(ERR, "Failed to initialize\n");
                        }

                        base += range_len;
                        len -= range_len;
                }

                initialized++;

                if (base == ceil) {
                        continue;
                }

                uintptr_t old_base = base;

                struct ARC_PFreelistNode *node = (struct ARC_PFreelistNode *)base;
                for (; base < ceil; base += PAGE_SIZE) {
                        node = (struct ARC_PFreelistNode *)base;
                        node->next = (struct ARC_PFreelistNode *)(base + PAGE_SIZE);
                }
                *fast_page_count += len >> PAGE_SIZE_LOWEST_EXPONENT;

                node->next = *fast_page_pool;
                *fast_page_pool = (struct ARC_PFreelistNode *)old_base;

                ARC_DEBUG(INFO, "\tInitialized 0x%"PRIx64" -> 0x%"PRIx64" as fast pages\n", old_base, ceil);
        }

        return initialized;
}

int init_pmm(struct ARC_MMap *mmap, int entries) {
        if (mmap == NULL || entries == 0) {
                ARC_DEBUG(ERR, "No memory map entries\n");
                ARC_HANG;
        }

        max_address_width = arch_physical_address_width();

        ARC_DEBUG(INFO, "Initializing PMM (%lu bit)\n", max_address_width);

        if (max_address_width < pmm_bias_count_high) {
                ARC_DEBUG(ERR, "More biases for high memory than bits in address width, only using first %lu biases\n", max_address_width);
                ARC_DEBUG(WARN, "Ignoring %lu biases\n", pmm_bias_count_high - max_address_width);
                pmm_bias_count_high = max_address_width;
        }

        if (max_address_width < pmm_bias_count_low) {
                ARC_DEBUG(ERR, "More biases for low memory than bits in address width, only using first %lu biases\n", max_address_width);
                ARC_DEBUG(WARN, "Ignoring %lu biases\n", pmm_bias_count_low - max_address_width);
                pmm_bias_count_low = max_address_width;
        }

        uintptr_t watermark_base = 0;

        size_t watermark_size = 2 * PAGE_SIZE;
        size_t pfreelist_size = max_address_width * sizeof(struct ARC_PFreelist);
        size_t pbuddy_size = max_address_width * sizeof(struct ARC_PBuddy);
        watermark_size += ALIGN(pfreelist_size, PAGE_SIZE) * 2;
        watermark_size += ALIGN(pbuddy_size, PAGE_SIZE) * 2;

        for (int i = 0; i < entries; i++) {
                struct ARC_MMap entry = mmap[i];

                if (entry.type != ARC_MEMORY_AVAILABLE || entry.len < watermark_size || entry.base < ARC_PMM_LOW_MEM_LIM) {
                        continue;
                }

                watermark_base = ARC_PHYS_TO_HHDM(entry.base);

                if (entry.len == watermark_size) {
                        mmap[i].type = ARC_MEMORY_RESERVED;
                } else {
                        mmap[i].base += watermark_size;
                        mmap[i].len -= watermark_size;
                }

                break;
        }

        if (init_vwatermark(&pmm_init_alloc, &pmm_init_alloc_meta, watermark_base, watermark_size) != 0) {
                ARC_HANG;
        }

        pmm_freelists_high = vwatermark_alloc(&pmm_init_alloc, pfreelist_size);
        memset(pmm_freelists_high, 0, pfreelist_size);
        pmm_buddies_high = vwatermark_alloc(&pmm_init_alloc, pbuddy_size);
        memset(pmm_buddies_high, 0, pbuddy_size);

        pmm_freelists_low = vwatermark_alloc(&pmm_init_alloc, pfreelist_size);
        memset(pmm_freelists_low, 0, pfreelist_size);
        pmm_buddies_low = vwatermark_alloc(&pmm_init_alloc, pbuddy_size);
        memset(pmm_buddies_low, 0, pbuddy_size);

        if (pmm_create_freelists(mmap, entries) == 0) {
                ARC_DEBUG(ERR, "Failed to create freelists\n");
                ARC_HANG;
        }

        return 0;
}
