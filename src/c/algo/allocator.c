/**
 * @file allocator.c
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
 * Implementation of functions to allocate data structures for other memory management
 * algorithms.
*/
#include <mm/algo/allocator.h>
#include <mm/algo/slab.h>
#include <mm/pmm.h>
#include <global.h>

static struct ARC_SlabMeta meta = { 0 };

void *ialloc(size_t size) {
	return slab_alloc(&meta, size);
}

void *icalloc(size_t size, size_t count) {
	return slab_alloc(&meta, size * count);
}

void *ifree(void *address) {
	return slab_free(&meta, address);
}

void *irealloc(void *address, size_t size) {
	(void)address;
	(void)size;

	ARC_DEBUG(ERR, "Unimplemented Arc_Realloc\n");

	return NULL;
}

int iallocator_expand(size_t pages) {
	int cumulative_err = (slab_expand(&meta, 0, pages) == 0);
	cumulative_err += (slab_expand(&meta, 1, pages) == 0);
	cumulative_err += (slab_expand(&meta, 2, pages) == 0);
	cumulative_err += (slab_expand(&meta, 3, pages) == 0);
	cumulative_err += (slab_expand(&meta, 4, pages) == 0);
	cumulative_err += (slab_expand(&meta, 5, pages) == 0);
	cumulative_err += (slab_expand(&meta, 6, pages) == 0);
	cumulative_err += (slab_expand(&meta, 7, pages) == 0);

	return cumulative_err;
}

int init_iallocator(size_t pages) {
	size_t range_length = (pages << 12) * 8;
	void *range = (void *)pmm_contig_alloc(pages * 8);

	return init_slab(&meta, range, range_length, 0) != range + range_length;
}
