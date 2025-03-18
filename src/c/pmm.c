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
*/
#include <mm/algo/allocator.h>
#include <arctan.h>
#include <global.h>
#include <mm/algo/pfreelist.h>
#include <mm/algo/vbuddy.h>
#include <mm/pmm.h>
#include <stdint.h>
#include <config.h>

static struct ARC_PFreelistMeta *arc_physical_mem = NULL;
static struct ARC_PFreelistMeta *arc_physical_low_mem = NULL;

struct vbuddy_list {
	struct ARC_VBuddyMeta *meta;
	struct vbuddy_list *next;
};

static struct vbuddy_list *arc_physical_contig_mem = NULL;

void *pmm_alloc_page() {
	return pfreelist_alloc(arc_physical_mem);
}

void *pmm_free_page(void *address) {
	return pfreelist_free(arc_physical_mem, address);
}

void *pmm_alloc_pages(size_t pages) {
	return pfreelist_contig_alloc(arc_physical_mem, pages);
}

void *pmm_free_pages(void *address, size_t pages) {
	return pfreelist_contig_free(arc_physical_mem, address, pages);
}

void *pmm_low_alloc_page() {
	return pfreelist_alloc(arc_physical_low_mem);
}

void *pmm_low_free_page(void *address) {
	return pfreelist_free(arc_physical_low_mem, address);
}

void *pmm_low_alloc_pages(size_t pages) {
	return pfreelist_contig_alloc(arc_physical_low_mem, pages);
}

void *pmm_low_free_pages(void *address, size_t pages) {
	return pfreelist_contig_free(arc_physical_low_mem, address, pages);
}

void *pmm_alloc(size_t size) {
	struct vbuddy_list *current = arc_physical_contig_mem;
	void *a = NULL;

	
	while (current != NULL && a == NULL) {
		a = vbuddy_alloc(current->meta, size);
		current = current->next;
	}

	return a;
}

void *pmm_free(void *address) {
	struct vbuddy_list *current = arc_physical_contig_mem;
	size_t size = 0;

	while (current != NULL && size == 0) {
		size = vbuddy_free(current->meta, address);
		current = current->next;
	}

	return (size > 0 ? address : NULL);
}

int init_pmm_contig() {
	ARC_DEBUG(INFO, "Initializing physically contiguous banks\n");

	struct ARC_PFreelistMeta *current = arc_physical_mem;
	struct vbuddy_list *last_elem = NULL;

	while (current != NULL) {
		struct vbuddy_list *element = ialloc(sizeof(*element));

		if (element == NULL) {
			ARC_DEBUG(ERR, "\tFailed to allocate new bank metadata\n");
			return -1;
		}

		size_t size = (current->free_objects * ARC_PMM_BUDDY_RATIO) * PAGE_SIZE;
		SIZE_T_NEXT_POW2(size);
		size >>= 1;

		void *base = pfreelist_contig_alloc(current, size / PAGE_SIZE);

		if (base == NULL) {
			ARC_DEBUG(ERR, "\tFailed to allocate region for bank\n");
			return -2;
		}

		if (init_vbuddy(element->meta, base, size, PAGE_SIZE) != 0) {
			ARC_DEBUG(ERR, "\tFailed to initialize bank\n");
			return -2;
		}

		if (last_elem == NULL) {
			arc_physical_contig_mem = element;
		} else {
			last_elem->next = element;
		}
		
		last_elem = element;

		current = current->next;

		ARC_DEBUG(INFO, "\tInitialized bank at %p for %lu bytes\n", base, size);
	}

	return 0;
}

int init_pmm(struct ARC_MMap *mmap, int entries) {
	if (mmap == NULL || entries == 0) {
		ARC_DEBUG(ERR, "Failed to initialize 64-bit PMM (one or more are NULL: %p %d)\n", mmap, entries);
		ARC_HANG;
	}

	mmap = (struct ARC_MMap *)ARC_PHYS_TO_HHDM(mmap);

	arc_physical_mem = (struct ARC_PFreelistMeta *)ARC_PHYS_TO_HHDM(Arc_BootMeta->pmm_state);
	arc_physical_low_mem = (struct ARC_PFreelistMeta *)ARC_PHYS_TO_HHDM(Arc_BootMeta->pmm_low_state);

	ARC_DEBUG(INFO, "Converting bootstrap allocator to use HHDM addresses\n");
	
	// All addresses in the meta remain physical, need to
	// convert to HHDM addresses (except for head)
	struct ARC_PFreelistMeta *current = arc_physical_low_mem;
	while (current != NULL) {
		current->base = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(current->base);
		current->ceil = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(current->ceil);
		// Head may an HHDM address if the list has been used
		// therefore ignore the upper 32-bits and convert it to
		// an HHDM address anyway
		current->head = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(((uint64_t)current->head) & UINT32_MAX);
		
		if (current->next != NULL) {
			current->next = (struct ARC_PFreelistMeta *)ARC_PHYS_TO_HHDM(current->next);
		}	
		
		
		current = current->next;
	}
	
	ARC_DEBUG(INFO, "Converted low: { B:%p C:%p H:%p SZ:%lu }\n", arc_physical_low_mem->base, arc_physical_low_mem->ceil, arc_physical_low_mem->head, arc_physical_low_mem->object_size);

	current = arc_physical_mem;
	struct ARC_PFreelistMeta *highest_meta = arc_physical_mem;
	while (current != NULL) {
		current->base = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(current->base);
		current->ceil = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(current->ceil);
		// Head may an HHDM address if the list has been used
		// therefore ignore the upper 32-bits and convert it to
		// an HHDM address anyway
		current->head = (struct ARC_PFreelistNode *)ARC_PHYS_TO_HHDM(((uint64_t)current->head) & UINT32_MAX);
		
		if (current->next != NULL) {
			current->next = (struct ARC_PFreelistMeta *)ARC_PHYS_TO_HHDM(current->next);
		}

		highest_meta = current;
		current = current->next;
	}

	ARC_DEBUG(INFO, "Converted high: { B:%p C:%p H:%p SZ:%lu }\n", arc_physical_mem->base, arc_physical_mem->ceil, arc_physical_mem->head, arc_physical_mem->object_size);

	uint64_t highest_alloc = (uint64_t)highest_meta->ceil;

	ARC_DEBUG(INFO, "Highest allocatable address: 0x%"PRIx64"\n", highest_alloc);

	for (int i = 0; i < entries; i++) {
		struct ARC_MMap entry = mmap[i];

		ARC_DEBUG(INFO, "\t%3d : 0x%016"PRIx64" -> 0x%016"PRIx64" (0x%016"PRIx64" bytes) | (%d)\n", i, entry.base, entry.base + entry.len, entry.len, entry.type);

		if (ARC_PHYS_TO_HHDM(entry.base) < highest_alloc || entry.type != ARC_MEMORY_AVAILABLE) {
			// Entry is below the highest allocation or it is not available
			// then skip
			continue;
		}

		ARC_DEBUG(INFO, "\t\tEntry is not in list, adding\n");

		// Round up
		uintptr_t base = ALIGN(entry.base, PAGE_SIZE);
		// Round down
		uintptr_t ceil = ((entry.base + entry.len) >> 12) << 12;

		// Found a memory entry that is not yet in the allocator
		struct ARC_PFreelistMeta *list = init_pfreelist(ARC_PHYS_TO_HHDM(base), ARC_PHYS_TO_HHDM(ceil), PAGE_SIZE);

		int ret = link_pfreelists(highest_meta, list);
		if (ret != 0) {
			ARC_DEBUG(ERR, "\t\tFailed to link lists (%d)\n", ret);
			continue;
		}

		ARC_DEBUG(INFO, "\t\tAdded\n");

		highest_meta = list;
	}

	ARC_DEBUG(INFO, "Finished setting up PMM\n");

	return 0;
}
