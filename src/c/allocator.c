/**
 * @file allocator.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan-OS/Kmm - Operating System Kernel Memory Manager
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan-OS/Kmm.
 *
 * Arctan-OS/Kmm is free software; you can redistribute it and/or
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
 * Implementation of the general purpose kernel allocator. It initializes a pslab
 * allocator to allow for allocation of objects below PAGE_SIZE and defaults to
 * the PMM for PAGE_SIZE or larger allocations.
*/
#include "global.h"
#include "mm/algo/pslab.h"
#include "mm/allocator.h"
#include "mm/pmm.h"

static struct ARC_PSlab meta = { 0 };

void *alloc(size_t size) {
	if (size > PAGE_SIZE / 2) {
		return pmm_alloc(max(PAGE_SIZE, size));
	}

	return pslab_alloc(&meta, size);
}

void *calloc(size_t size, size_t count) {
	size_t final = size * count;

	if (final > PAGE_SIZE / 2) {
		return pmm_alloc(max(PAGE_SIZE, size));
	}

	return pslab_alloc(&meta, final);
}

size_t free(void *address) {
	size_t ret = pslab_free(&meta, address);

	if (ret == 0 && address != NULL) {
		ret = pmm_free(address);
	}

	if (ret == 0) {
		ARC_DEBUG(ERR, "Failed to free %p\n", address);
	}

	return ret;
}

int allocator_expand(size_t pages) {
	return pslab_expand(&meta, pages);
}

int init_allocator(size_t pages) {
	return init_pslab(&meta, 4, pages);
}
