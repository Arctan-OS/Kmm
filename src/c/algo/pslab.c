/**
 * @file pslab.c
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
#include <mm/algo/pslab.h>
#include <mm/algo/pfreelist.h>
#include <mm/pmm.h>
#include <global.h>
#include <lib/util.h>

void *pslab_alloc(struct ARC_PSlabMeta *meta, size_t size) {
	if (size > meta->list_sizes[7]) {
		// Just allocate a contiguous set of pages
		ARC_DEBUG(ERR, "Failed to allocate size %lu\n", size);
		return NULL;
	}

	for (int i = 0; i < 8; i++) {
		if (size <= meta->list_sizes[i]) {
			return pfreelist_alloc(meta->lists[i]);
		}
	}

	return NULL;
}

void *pslab_free(struct ARC_PSlabMeta *meta, void *address) {
	for (int i = 0; i < 8; i++) {
		void *base = meta->lists[i]->base;
		void *ceil = meta->lists[i]->ceil;

		if (base <= address && address <= ceil) {
			memset(address, 0, meta->list_sizes[i]);
			return pfreelist_free(meta->lists[i], address);
		}
	}

	if ((meta->attributes & 1) == 0) {
		ARC_DEBUG(ERR, "Failed to free %p\n", address);
	}

	return NULL;
}

int pslab_expand(struct ARC_PSlabMeta *pslab, int list, size_t pages) {
	if (pslab == NULL || list < 0 || list > 7 || pages == 0) {
		return -1;
	}

	uint64_t base = (uint64_t)pmm_alloc(pages * PAGE_SIZE);
	struct ARC_PFreelistMeta *meta = init_pfreelist(base, base + (pages * PAGE_SIZE), pslab->list_sizes[list]);

	ARC_DEBUG(INFO, "Expanding SLAB %p (%d) by %lu pages\n", pslab, list, pages);

	return link_pfreelists(pslab->lists[list], meta);
}

void *init_pslab(struct ARC_PSlabMeta *meta, void *range, size_t range_size, uint32_t attributes) {
	ARC_DEBUG(INFO, "Initializing SLAB allocator in range %p (%lu)\n", range, range_size);

	size_t partition_size = range_size >> 3;
	size_t object_size = 16;
	uint64_t base = (uint64_t)range;

	meta->lists[0] = init_pfreelist(base, base + partition_size, object_size);
	meta->list_sizes[0] = object_size;
	object_size <<= 1;
	base += partition_size;
	meta->lists[1] = init_pfreelist(base, base + partition_size, object_size);
	meta->list_sizes[1] = object_size;
	object_size <<= 1;
	base += partition_size;
	meta->lists[2] = init_pfreelist(base, base + partition_size, object_size);
	meta->list_sizes[2] = object_size;
	object_size <<= 1;
	base += partition_size;
	meta->lists[3] = init_pfreelist(base, base + partition_size, object_size);
	meta->list_sizes[3] = object_size;
	object_size <<= 1;
	base += partition_size;
	meta->lists[4] = init_pfreelist(base, base + partition_size, object_size);
	meta->list_sizes[4] = object_size;
	object_size <<= 1;
	base += partition_size;
	meta->lists[5] = init_pfreelist(base, base + partition_size, object_size);
	meta->list_sizes[5] = object_size;
	object_size <<= 1;
	base += partition_size;
	meta->lists[6] = init_pfreelist(base, base + partition_size, object_size);
	meta->list_sizes[6] = object_size;
	object_size <<= 1;
	base += partition_size;
	meta->lists[7] = init_pfreelist(base, base + partition_size, object_size);
	meta->list_sizes[7] = object_size;
	object_size <<= 1;
	base += partition_size;

	meta->range = range;
	meta->range_length = range_size;
	meta->attributes = attributes;

	ARC_DEBUG(INFO, "Initialized SLAB allocator\n");

	return (void *)base;
}
