/**
 * @file pslab.c
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
 * Implementation of a SLAB sort memory management algorithm that operates
 * on present memory regions. This is similar to the PMM's biases and buddy
 * allocators, except that all possible object sizes are present and the
 * depth to which it works is restricted to 8 contiguous exponents.
 *
 * NOTE: This algorithm depends on the PMM being initialized as it utilizes
 *       `pmm_alloc` to attain memory for its lists.
*/
#include <mm/algo/pslab.h>
#include <mm/algo/pfreelist.h>
#include <mm/pmm.h>
#include <global.h>
#include <lib/util.h>

void *pslab_alloc(struct ARC_PSlab *meta, size_t size) {
	if (meta == NULL) {
		return NULL;
	}

	SIZE_T_NEXT_POW2(size);
	int exp = max(__builtin_ctz(size), meta->lowest_exp);

	if (exp > meta->lowest_exp + 8) {
		return NULL;
	}

	retry:;

	exp -= meta->lowest_exp;

	void *a = pfreelist_alloc(&meta->lists[exp]);

	if (a == NULL && pslab_expand(meta, 1) > exp) {
		goto retry;
	}

	return a;
}

size_t pslab_free(struct ARC_PSlab *meta, void *address) {
	if (meta == NULL || address == NULL) {
		return 0;
	}

	for (int i = 0; i < 8; i++) {
		if (pfreelist_free(&meta->lists[i], address) == address) {
			return 1 << (meta->lowest_exp + i);
		}
	}

	return 0;
}

int pslab_expand(struct ARC_PSlab *meta, size_t pages_per_list) {
	if (meta == NULL || pages_per_list == 0) {
		return -1;
	}

	for (int i = 0; i < 8; i++) {
		size_t object_size = 1 << (meta->lowest_exp + i);
		size_t size = pages_per_list << PAGE_SIZE_LOWEST_EXPONENT;
		uintptr_t base = (uintptr_t)pmm_alloc(size);

		if (base == 0) {
			// It isn't a fatal error (at least immediately) that more memory could
			// not be allocated for any given list (it is not required for all lists to
			// have the same number of elements) but an early return is needed (as a
			// pfreelist cannot be initialized in memory we don't have) and the current
			// index is returned as the error code so that pslab_alloc can determine
			// whether to attempt to allocate again or to return NULL.
			ARC_DEBUG(WARN, "Failed to allocate more space for list %d in pslab, exiting\n");
			return i;
		}

		init_pfreelist(&meta->lists[i], base, base + size, object_size);
	}

	return 0;
}

int init_pslab(struct ARC_PSlab *meta, int lowest_exp, size_t pages_per_list) {
	if (meta == NULL || pages_per_list == 0 || lowest_exp < __builtin_ctz(sizeof(void *))) {
		ARC_DEBUG(ERR, "Failed to initialize pslab: not enough space for pointers\n");
		return -1;
	}

	meta->lowest_exp = lowest_exp;

	return pslab_expand(meta, pages_per_list);
}
