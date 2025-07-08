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
#include "util.h"
#include <mm/algo/pbuddy.h>
#include <arch/info.h>
#include <mm/pmm.h>
#include <arch/x86-64/config.h>
#include <global.h>
#include <lib/util.h>
#include <mm/algo/pfreelist.h>
#include <mm/algo/vwatermark.h>
#include <stdint.h>

static const uint32_t pmm_bias_array[] = { PMM_BIAS_ARRAY };
static const uintmax_t pmm_bias_low[] = { PMM_BIAS_LOW };
static const uint32_t pmm_bias_ratio[] = { PMM_BIAS_RATIO };
static uint32_t pmm_bias_count = sizeof(pmm_bias_array) / sizeof(uint32_t);

STATIC_ASSERT(sizeof(pmm_bias_array) / sizeof(uint32_t) == sizeof(pmm_bias_low) / sizeof(uintmax_t), "The number of bias array elements is not equal to the number bias low elements!");
STATIC_ASSERT(sizeof(pmm_bias_array) / sizeof(uint32_t) == sizeof(pmm_bias_ratio) / sizeof(uint32_t), "The number of bias array elements is not equal to the number of bias ratio elements!");

static uint32_t max_address_width = 0;
static struct ARC_VWatermarkMeta pmm_init_alloc = { 0 };
static struct ARC_PFreelist *pmm_freelists = NULL;
static struct ARC_PBuddy *pmm_buddies = NULL;

// This is a pool of fast pages. When you need an O(1) allocation of a
// the PAGE_SIZE, use this by calling pmm_fast_page_alloc and USE pmm_fast_page_free.
// Fast pages is a non-contiguous freelist with an object size that of the smallest
// power two page size
struct ARC_PFreelistNode *fast_page_pool = NULL;
static size_t fast_page_count = 0;
static size_t fast_page_allocated = 0;

void *pmm_alloc(size_t size) {
        int size_exponent = __builtin_ctz(size);
//        int size_bias_exponent = -1;

        if (size_exponent == SMALLEST_PAGE_SIZE_EXPONENT) {
                return pmm_fast_page_alloc();
        }

        if (pmm_freelists[size_exponent].head != NULL) {
                return pfreelist_alloc(&pmm_freelists[size_exponent]);
        }

        SIZE_T_NEXT_POW2(size);

        return NULL;
}

size_t pmm_free(void *address) {
        if (address == NULL) {
                return 0;
        }

        for (int i = 0; i < pmm_bias_count; i++) {
                int bias = pmm_bias_array[i];

                if (pmm_freelists[bias].head != NULL && pfreelist_free(&pmm_freelists[bias], address) == address) {
                        return (1 << bias);
                }
        }

        pmm_fast_page_free(address);

        return 0;
}

size_t pmm_alloc_fast_pages(size_t count) {
        fast_page_count += count;
        return 0;
}

void *pmm_fast_page_alloc() {
        if (fast_page_count == fast_page_allocated) {
                pmm_alloc_fast_pages(16);
        }

        ARC_ATOMIC_INC(fast_page_allocated);
        void *a = fast_page_pool;
        fast_page_pool = fast_page_pool->next;

        return a;
}

size_t pmm_fast_page_free(void *address) {
        if (address == NULL) {
                return 0;
        }

        struct ARC_PFreelistNode *entry = (struct ARC_PFreelistNode *)address;
        fast_page_pool = entry;
        entry->next = fast_page_pool;
        ARC_ATOMIC_DEC(fast_page_allocated);

        return PAGE_SIZE;
}

void *pmm_low_page_alloc() {
        return NULL;
}

size_t pmm_low_page_free(void *address) {
        return 0;
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

                ARC_DEBUG(INFO, "Found entry 0x%"PRIx64" -> 0x%"PRIx64" to initialize\n", base, ceil);

                for (int j = 0; j < pmm_bias_count; j++) {
                        if (pmm_bias_ratio[j] == 0) {
                                continue;
                        }

                        int bias = pmm_bias_array[j];
                        size_t object_size = 1 << bias;

                        if (len < pmm_bias_low[j] * object_size) {
                                continue;
                        }

                        // Initialize list
                        size_t range_len = ALIGN(len * pmm_bias_ratio[j] / PMM_BIAS_DENOMINATOR, object_size);
                        if (range_len > len) {
                                range_len = (len >> bias) << bias;
                        }
                        struct ARC_PFreelistMeta *meta = init_pfreelist(base, base + range_len, object_size);
                        pfreelist_add(&pmm_freelists[bias], meta);

                        base += range_len;
                        len -= range_len;
                }

                for (int j = 0; j < pmm_bias_count; j++) {
                        if (pmm_bias_ratio[j] != 0) {
                                continue;
                        }

                        int bias = pmm_bias_array[j];
                        size_t object_size = 1 << bias;

                        if (len < pmm_bias_low[j] * object_size) {
                                continue;
                        }

                        // Initialize list
                        size_t range_len = ALIGN(len - object_size, object_size);
                        if (range_len > len) {
                                range_len = (len >> bias) << bias;
                        }
                        struct ARC_PFreelistMeta *meta = init_pfreelist(base, base + range_len, object_size);
                        pfreelist_add(&pmm_freelists[bias], meta);

                        base += range_len;
                        len -= range_len;
                }

                struct ARC_PFreelistNode *node = (struct ARC_PFreelistNode *)base;
                for (; base < ceil; base += PAGE_SIZE) {
                        node = (struct ARC_PFreelistNode *)base;
                        node->next = (struct ARC_PFreelistNode *)(base + PAGE_SIZE);
                }
                fast_page_count += len >> SMALLEST_PAGE_SIZE_EXPONENT;

                node->next = fast_page_pool;
                fast_page_pool = node;

                ARC_DEBUG(INFO, "\tInitialized 0x%"PRIx64" -> 0x%"PRIx64" as fast pages\n", base, ceil);

                initialized++;
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

        if (max_address_width < pmm_bias_count) {
                ARC_DEBUG(ERR, "More biases than address width, only using first %lu biases\n", max_address_width);
                ARC_DEBUG(WARN, "Ignoring %lu biases\n", pmm_bias_count - max_address_width);
                pmm_bias_count = max_address_width;
        }

        uintptr_t watermark_base = 0;

        size_t watermark_size = 2 * PAGE_SIZE;
        size_t pfreelist_size = max_address_width * sizeof(struct ARC_PFreelist);
        size_t pbuddy_size = max_address_width * sizeof(struct ARC_PBuddy);
        watermark_size += ALIGN(pfreelist_size, PAGE_SIZE);
        watermark_size += ALIGN(pbuddy_size, PAGE_SIZE);

        for (int i = 0; i < entries; i++) {
                struct ARC_MMap entry = mmap[i];

                if (entry.type != ARC_MEMORY_AVAILABLE && entry.len < watermark_size) {
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

        if (init_vwatermark(&pmm_init_alloc, watermark_base, watermark_size) != 0) {
                ARC_HANG;
        }

        pmm_freelists = vwatermark_alloc(&pmm_init_alloc, pfreelist_size);
        memset(pmm_freelists, 0, pfreelist_size);
        pmm_buddies = vwatermark_alloc(&pmm_init_alloc, pbuddy_size);
        memset(pmm_freelists, 0, pbuddy_size);

        if (pmm_create_freelists(mmap, entries) == 0) {
                ARC_DEBUG(ERR, "Failed to create freelists\n");
                ARC_HANG;
        }

        return 0;
}
