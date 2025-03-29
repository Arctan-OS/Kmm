/**
 * @file pmm.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan.
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
 * 
 * The resulting physical memory map looks like:
 * |--------|-1-|----A----|----------------|---2---|--3--|--4--|-----A-----|--------------|
 *            1MB
 * 1: Bootstrapper, Kernel, and Initramfs
 * 2: Arbitrary reserved section
 * 3: Possible paging tables to switch from 32 bits to 64
 * 4: Internal allocator
 * A: Vbuddy contiguous allocator
 * - (with no character interruption): Freelist
 * 
 * Low memory (memory under 1 MB) is kept as its own freelist unless ARC_SEPARATE_LOW_MEM
 * is set to 0 (WIP).
*/
#include <arch/x86-64/util.h>
#include <mm/bank.h>
#include <mm/iallocator.h>
#include <arctan.h>
#include <global.h>
#include <mm/algo/pfreelist.h>
#include <mm/algo/vbuddy.h>
#include <mm/pmm.h>
#include <stdint.h>
#include <config.h>

static struct ARC_BankMeta *low_bank_meta = NULL;
static struct ARC_BankMeta *high_bank_meta = NULL;
static struct ARC_BankMeta *high_contig_bank_meta = NULL;

// NOTE: The pfreelist and vbuddy do check to ensure that the address
//       being freed is within its meta; however, it would be nice to
//       not rely on that and narrow the correct meta down within these
//       free functions

void *pmm_alloc_page() {
	struct ARC_BankNode *node = high_bank_meta->first;
	void *a = NULL;
	while (node != NULL && a == NULL) {
		a = pfreelist_alloc(node->allocator_meta);
		node = node->next;
	}

	return a;
}

void *pmm_free_page(void *address) {
	struct ARC_BankNode *node = high_bank_meta->first;
	void *a = NULL;
	while (node != NULL && a == NULL) {
		a = pfreelist_free(node->allocator_meta, address);
		node = node->next;
	}

	return a;
}

void *pmm_alloc_pages(size_t pages) {
	struct ARC_BankNode *node = high_bank_meta->first;
	void *a = NULL;
	while (node != NULL && a == NULL) {
		a = pfreelist_contig_alloc(node->allocator_meta, pages);
		node = node->next;
	}

	return a;
}

void *pmm_free_pages(void *address, size_t pages) {
	struct ARC_BankNode *node = high_bank_meta->first;
	void *a = NULL;
	while (node != NULL && a == NULL) {
		a = pfreelist_contig_free(node->allocator_meta, address, pages);
		node = node->next;
	}

	return a;
}

void *pmm_low_alloc_page() {
	struct ARC_BankNode *node = low_bank_meta->first;
	void *a = NULL;
	while (node != NULL && a == NULL) {
		a = pfreelist_alloc(node->allocator_meta);
		node = node->next;
	}

	return a;
}

void *pmm_low_free_page(void *address) {
	struct ARC_BankNode *node = low_bank_meta->first;
	void *a = NULL;
	while (node != NULL && a == NULL) {
		a = pfreelist_free(node->allocator_meta, address);
		node = node->next;
	}

	return a;
}

void *pmm_low_alloc_pages(size_t pages) {
	struct ARC_BankNode *node = low_bank_meta->first;
	void *a = NULL;
	while (node != NULL && a == NULL) {
		a = pfreelist_contig_alloc(node->allocator_meta, pages);
		node = node->next;
	}

	return a;
}

void *pmm_low_free_pages(void *address, size_t pages) {
	struct ARC_BankNode *node = low_bank_meta->first;
	void *a = NULL;
	while (node != NULL && a == NULL) {
		a = pfreelist_contig_free(node->allocator_meta, address, pages);
		node = node->next;
	}

	return a;
}

void *pmm_alloc(size_t size) {
	struct ARC_BankNode *node = high_contig_bank_meta->first;
	void *a = NULL;

	while (node != NULL && a == NULL) {
		a = vbuddy_alloc(node->allocator_meta, size);
		node = node->next;
	}

	return a;
}

size_t pmm_free(void *address) {
	struct ARC_BankNode *node = high_contig_bank_meta->first;
	size_t size = 0;

	while (node != NULL && size == 0) {
		size = vbuddy_free(node->allocator_meta, address);
		node = node->next;
	}

	return size;
}

static uintptr_t pmm_convert_banks_to_hhdm() {
	high_bank_meta = (struct ARC_BankMeta *)ARC_PHYS_TO_HHDM(Arc_BootMeta->pmm_high_bank);
	low_bank_meta = (struct ARC_BankMeta *)ARC_PHYS_TO_HHDM(Arc_BootMeta->pmm_low_bank);

	ARC_DEBUG(INFO, "Converting bootstrap allocator to use HHDM addresses\n");

	struct ARC_BankNode *node = (struct ARC_BankNode *)ARC_PHYS_TO_HHDM(high_bank_meta->first);
	uintptr_t highest_ceil = 0;
	while (node != NULL) {
		struct ARC_PFreelistMeta *meta = (struct ARC_PFreelistMeta *)ARC_PHYS_TO_HHDM(node->allocator_meta);
		node->allocator_meta = meta;

		meta->base = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(meta->base);
		meta->ceil = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(meta->ceil);
		
		if ((uintptr_t)meta->ceil > highest_ceil) {
			highest_ceil = (uintptr_t)meta->ceil;
		}

		meta->head = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(((uint64_t)meta->head) & UINT32_MAX);
		
		if (node->next != NULL) {
			node->next = (struct ARC_BankNode *)ARC_PHYS_TO_HHDM(node->next);
		}
		
		node = node->next;
	}
	
	node = (struct ARC_BankNode *)ARC_PHYS_TO_HHDM(low_bank_meta->first);
	while (node != NULL) {
		struct ARC_PFreelistMeta *meta = (struct ARC_PFreelistMeta *)ARC_PHYS_TO_HHDM(node->allocator_meta);
		node->allocator_meta = meta;

		meta->base = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(meta->base);
		meta->ceil = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(meta->ceil);
		meta->head = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(((uint64_t)meta->head) & UINT32_MAX);
		
		if (node->next != NULL) {
			node->next = (struct ARC_BankNode *)ARC_PHYS_TO_HHDM(node->next);
		}

		node = node->next;
	}

	return highest_ceil;
}

static int pmm_convert_to_dynamic_banks() {
	struct ARC_BankMeta *dynamic_high = init_bank(ARC_BANK_TYPE_PFREELIST, ARC_BANK_USE_ALLOC_INTERNAL);
	
	if (dynamic_high == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate high dynamic bank\n");
		return -1;
	}

	struct ARC_BankMeta *dynamic_low = init_bank(ARC_BANK_TYPE_PFREELIST, ARC_BANK_USE_ALLOC_INTERNAL);

	if (dynamic_low == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate low dynamic bank\n");
		ifree(dynamic_high);
		return -2;
	}

	struct ARC_BankNode *node = high_bank_meta->first;
	while (node != NULL) {
		bank_add(dynamic_high, node->allocator_meta);
		node = node->next;
	}

	node = low_bank_meta->first;
	while (node != NULL) {
		bank_add(dynamic_low, node->allocator_meta);
		node = node->next;
	}

	high_bank_meta = dynamic_high;
	low_bank_meta = dynamic_low;

	ARC_DEBUG(INFO, "Migrated banks to dynamic memory\n");

	return 0;
}

static uintptr_t pmm_init_missed_memory(struct ARC_MMap *mmap, int entries, uintptr_t highest_ceil) {
	ARC_DEBUG(INFO, "Initializing possibly missed memory:\n");
	uintptr_t ret = highest_ceil;

	for (int i = 0; i < entries; i++) {
		struct ARC_MMap entry = mmap[i];

		ARC_DEBUG(INFO, "\t%3d : 0x%016"PRIx64" -> 0x%016"PRIx64" (0x%016"PRIx64" bytes) | (%d)\n", i, entry.base, entry.base + entry.len, entry.len, entry.type);

		if (ARC_PHYS_TO_HHDM(entry.base) < highest_ceil || entry.type != ARC_MEMORY_AVAILABLE) {
			// Entry is below the highest allocation or it is not available
			// then skip
			continue;
		}

		ARC_DEBUG(INFO, "\t\tEntry is not in list, adding\n");

		// Round up
		uintptr_t base = ARC_PHYS_TO_HHDM(ALIGN(entry.base, PAGE_SIZE));
		// Round down to page size
		uintptr_t ceil = ARC_PHYS_TO_HHDM((entry.base + entry.len) & (~0xFFF));

		if (ceil > ret) {
			ret = ceil;
		}

		// Found a memory entry that is not yet in the allocator
		struct ARC_PFreelistMeta *list = init_pfreelist(base, ceil, PAGE_SIZE);
		bank_add(high_bank_meta, list);
		
		ARC_DEBUG(INFO, "\t\tAdded\n");
	}

	ARC_DEBUG(INFO, "Initialized possibly missed memory\n");

	return ret;
}

static int pmm_init_contiguous_high_memory() {
	ARC_DEBUG(INFO, "Initializing contiguous memory regions\n");

	high_contig_bank_meta = init_bank(ARC_BANK_TYPE_VBUDDY, ARC_BANK_USE_ALLOC_INTERNAL);

	if (high_contig_bank_meta == NULL) {
		ARC_DEBUG(ERR, "Failed to initialize dynamic bank for contiguous allocations\n");
		return -1;
	}

	struct ARC_BankNode *node = high_bank_meta->first;
	int count = 0;

	while (node != NULL) {
		struct ARC_PFreelistMeta *freelist = node->allocator_meta;

		size_t size = (freelist->free_objects * ARC_PMM_BUDDY_RATIO) * PAGE_SIZE;
		SIZE_T_NEXT_POW2(size);
		size >>= 1;

		void *base = pfreelist_contig_alloc(freelist, size / PAGE_SIZE);

		if (base == NULL) {
			ARC_DEBUG(ERR, "\tFailed to allocate region for bank\n");
			return -2;
		}

		struct ARC_VBuddyMeta *meta = ialloc(sizeof(*meta));

		if (meta == NULL) {
			ARC_DEBUG(ERR, "\tFailed to allocate new contiguous meta\n");
			break;
		}

		meta->ialloc = ialloc;
		meta->ifree = ifree;

		if (init_vbuddy(meta, base, size, PAGE_SIZE) != 0) {
			ARC_DEBUG(ERR, "\tFailed to initialize bank\n");
			return -2;
		}

		if (bank_add(high_contig_bank_meta, meta) == NULL) {
			ARC_DEBUG(ERR, "\tCould not insert into bank list\n");
			break;
		}

		count++;

		node = node->next;

		ARC_DEBUG(INFO, "\tInitialized bank at %p for %lu bytes\n", base, size);
	}

	return count;
}

int init_pmm(struct ARC_MMap *mmap, int entries) {
	ARC_DEBUG(INFO, "Setting up PMM\n");

	if (mmap == NULL || entries == 0) {
		ARC_DEBUG(ERR, "Failed to initialize 64-bit PMM (one or more are NULL: %p %d)\n", mmap, entries);
		ARC_HANG;
	}

	uintptr_t highest_ceil = pmm_convert_banks_to_hhdm();

	if (highest_ceil == 0) {
		ARC_DEBUG(ERR, "Conversion of banks to HHDM failed as the highest allocatable address is 0\n");
		ARC_HANG;
	}

	ARC_DEBUG(INFO, "Highest allocatable address: 0x%"PRIx64"\n", highest_ceil);

	if (init_iallocator(256) != 0) {
		ARC_DEBUG(ERR, "Failed to initialize internal allocator\n");
		ARC_HANG;
	}
	ARC_DEBUG(INFO, "Initialized internal allocator\n");

	if (pmm_convert_to_dynamic_banks() != 0) {
		ARC_DEBUG(ERR, "Failed to convert to dynamic banks\n");
		ARC_HANG;
	}

	highest_ceil = pmm_init_missed_memory((struct ARC_MMap *)ARC_PHYS_TO_HHDM(mmap),entries, highest_ceil);
	ARC_DEBUG(INFO, "Highest allocatable address: 0x%"PRIx64"\n", highest_ceil);

	if (pmm_init_contiguous_high_memory() <= 0) {
		ARC_DEBUG(ERR, "Failed to initialize contiguous memory regions in high memory\n");
		ARC_HANG;
	}

	ARC_DEBUG(INFO, "Finished setting up PMM\n");

	return 0;
}
