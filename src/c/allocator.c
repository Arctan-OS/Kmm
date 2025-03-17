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
 * Implements functions used for in kernel allocations.
*/
#include <mm/allocator.h>
#include <mm/algo/pslab.h>
#include <mm/vmm.h>
#include <global.h>

static struct ARC_PSlabMeta meta = { 0 };

void *alloc(size_t size) {
	if (size > PAGE_SIZE / 2) {
		return vmm_alloc(max(PAGE_SIZE, size));
	}

	return pslab_alloc(&meta, size);
}

void *calloc(size_t size, size_t count) {
	size_t final = size * count;

	if (final > PAGE_SIZE / 2) {
		return vmm_alloc(max(PAGE_SIZE, size));
	}

	return pslab_alloc(&meta, final);
}

void *free(void *address) {
	void *ret = pslab_free(&meta, address);

	if (ret == NULL && address != NULL) {
		ret = vmm_free(address);
	}

	if (ret == NULL) {
		ARC_DEBUG(ERR, "Failed to free %p\n", address);
	}

	return ret;
}

void *realloc(void *address, size_t size) {
	(void)address;
	(void)size;

	ARC_DEBUG(ERR, "Unimplemented Arc_Realloc\n");

	return NULL;
}

int allocator_expand(size_t pages) {
	(void)pages;
	return 0;
}

int init_allocator(size_t pages) {
	size_t range_length = (pages << 12) * 8;
	void *range = (void *)vmm_alloc(range_length);

	if (range == NULL) {
		return -1;
	}

	// NOTE: Bit 0 is set here as the free function blindly pokes the SLAB allocator
	//       to see if it can free the given address. If it can't the SLAB allocator, properly
	//       reports an error. This is not desired here, as the allocation may be allocated using
	//       the VMM instead. The free function resorts to reporting its own error in the event
	//       it fails
	return init_pslab(&meta, range, range_length, 1) != range + range_length;
}
