/**
 * @file pfreelist.c
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
 * Implementation of a freelist memory management algorithm that operates
 * on present regions of memory. A freelist is set up within the given region
 * and constant sized objects are allocated and freed from/to it.
*/
#include <global.h>
#include <lib/atomics.h>
#include <lib/util.h>
#include <mm/algo/pfreelist.h>
#include <stdint.h>

#define ADDRESS_IN_META(address, meta) ((void *)meta->base <= (void *)address && (void *)address <= (void *)meta->ceil)

void *pfreelist_alloc(struct ARC_PFreelist *list) {
	if (list == NULL || list->head == NULL) {
		ARC_DEBUG(ERR, "No list provided or no head available, cannot allocate\n");
		return NULL;
	}

	spinlock_lock(&list->ordering_lock);

	struct ARC_PFreelistMeta *current = list->head;
	struct ARC_PFreelistMeta *last = NULL;

	while (current != NULL && current->free_objects == 0) {
		last = current;
		current = current->next;
	}

	if (last != NULL) {
		last->next = list->head;
		void *tmp = list->head->next;
		list->head->next = current->next;
		current->next = tmp;
		list->head = current;
	}

	spinlock_unlock(&list->ordering_lock);

	if (current == NULL) {
		return NULL;
	}

	register struct ARC_PFreelistNode **head = &current->head;
	struct ARC_PFreelistNode *ret = NULL;

	if (*head == NULL) {
		return NULL;
	}

	ARC_ATOMIC_XCHG(head, (struct ARC_PFreelistNode **)*head, &ret);

	return (void *)ret;
}

void *pfreelist_free(struct ARC_PFreelist *list, void *address) {
	if (list == NULL || list->head == NULL || address == NULL) {
		ARC_DEBUG(ERR, "List or address not provided or list head is NULL\n");
		return NULL;
	}

	spinlock_lock(&list->ordering_lock);

	struct ARC_PFreelistMeta *current = list->head;
	while (current != NULL && !ADDRESS_IN_META(address, current)) {
		current = current->next;
	}

	spinlock_unlock(&list->ordering_lock);

	if (current == NULL) {
		return NULL;
	}

	struct ARC_PFreelistNode *b = address;
	ARC_ATOMIC_XCHG(&current->head, &b, &b->next);
	ARC_ATOMIC_INC(current->free_objects);

	return address;
}

int init_pfreelist(struct ARC_PFreelist *list, uintptr_t _base, uintptr_t _ceil, size_t _object_size) {
	if (list == NULL || _base > _ceil || _object_size == 0) {
		// Invalid parameters
		return -1;
	}

	if (_ceil - _base < _object_size + sizeof(struct ARC_PFreelistMeta)) {
		// There is not enough space for one object
		return -2;
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

	spinlock_lock(&list->ordering_lock);
	meta->next = list->head;
	list->head = meta;
	spinlock_unlock(&list->ordering_lock);

	return 0;
}
