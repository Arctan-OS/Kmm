/**
 * @file pfreelist.c
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
*/
#include <mm/algo/pfreelist.h>
#include <lib/atomics.h>
#include <lib/util.h>
#include <global.h>
#include <stdint.h>

#define ADDRESS_IN_META(address, meta) ((void *)meta->base <= (void *)address && (void *)address <= (void *)meta->ceil)

// TODO: Locks
void *pfreelist_alloc(struct ARC_PFreelist *list) {
	if (list == NULL) {
		ARC_DEBUG(ERR, "No list provided, cannot allocate\n");
		return NULL;
	}

	struct ARC_PFreelistMeta *current = list->head;
	int retried = 0;

	retry:;

	if (current == NULL) {
		return NULL;
	}

	void *a = current->head;

	if (a == NULL) {
		current = current->next;
		retried++;
		goto retry;
	}

	ARC_ATOMIC_DEC(current->free_objects)
	current->head = current->head->next;

	if (retried > 0) {
		void *b = current->next;
		list->head->next = b;
		current->next = list->head;
		list->head = current;
	}

	return a;
}

// TODO: Locks
void *pfreelist_free(struct ARC_PFreelist *list, void *address) {
	if (list == NULL || address == NULL) {
		ARC_DEBUG(ERR, "List or address not provided\n");
		return NULL;
	}

	struct ARC_PFreelistMeta *current = list->head;
	while (current != NULL && !ADDRESS_IN_META(address, current)) {
		current = current->next;
	}

	if (current == NULL) {
		return NULL;
	}

	struct ARC_PFreelistNode *b = address;
	b->next = current->head;
	current->head = b;
	ARC_ATOMIC_INC(current->free_objects);

	return address;
}

int pfreelist_add(struct ARC_PFreelist *list, struct ARC_PFreelistMeta *meta) {
	if (list == NULL || meta == NULL) {
		return -1;
	}

	ARC_ATOMIC_XCHG(&list->head, &meta, &meta->next);

	return 0;
}

struct ARC_PFreelistMeta *init_pfreelist(uintptr_t _base, uintptr_t _ceil, size_t _object_size) {
	if (_base > _ceil || _object_size == 0) {
		// Invalid parameters
		return NULL;
	}

	if (_ceil - _base < _object_size + sizeof(struct ARC_PFreelistMeta)) {
		// There is not enough space for one object
		return NULL;
	}

	struct ARC_PFreelistMeta *meta = (struct ARC_PFreelistMeta *)_base;

	memset(meta, 0, sizeof(struct ARC_PFreelistMeta));

	init_static_spinlock(&meta->lock);

	// Number of objects to accomodate meta
	size_t objects = (sizeof(struct ARC_PFreelistMeta) / _object_size) + 1;
	_base += objects * _object_size;
	_ceil -= _object_size;

	struct ARC_PFreelistNode *base = (struct ARC_PFreelistNode *)_base;
	struct ARC_PFreelistNode *ceil = (struct ARC_PFreelistNode *)_ceil;

	// Store meta information
	meta->base = base;
	meta->head = base;
	meta->ceil = ceil;
	meta->free_objects = (_ceil - _base) / _object_size - objects + 1;

	ARC_DEBUG(INFO, "Creating pfreelist from %p to %p with %lu byte objects (%lu objects)\n", base, ceil, _object_size, meta->free_objects);

	// Initialize the linked list
	struct ARC_PFreelistNode *current = NULL;
	for (; _base < _ceil; _base += _object_size) {
		current = (struct ARC_PFreelistNode *)_base;
		current->next = (struct ARC_PFreelistNode *)(_base + _object_size);
	}

	// Set last entry to point to NULL
	current->next = NULL;

	return meta;
}
