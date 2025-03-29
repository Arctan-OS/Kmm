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
#include <mm/iallocator.h>
#include <mm/algo/pslab.h>
#include <mm/pmm.h>
#include <global.h>
#include <string.h>

static struct ARC_PSlabMeta meta = { 0 };

void *ialloc(size_t size) {
	return pslab_alloc(&meta, size);
}

void *icalloc(size_t size, size_t count) {
	return pslab_alloc(&meta, size * count);
}

size_t ifree(void *address) {
	return pslab_free(&meta, address);
}

int iallocator_expand(size_t pages) {
	return pslab_expand(&meta, pages);
}

int init_iallocator(size_t pages) {
	size_t range_length = (pages << 12);
	void *range = (void *)pmm_alloc_pages(pages);

	if (range == NULL) {
		return -1;
	}

	return init_pslab(&meta, range, range_length) != range + range_length;
}
