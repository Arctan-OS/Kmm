/**
 * @file pfreelist.c
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
#include <mm/algo/pfreelist.h>
#include <lib/atomics.h>
#include <lib/util.h>
#include <global.h>
#include <stdint.h>
#include <inttypes.h>

#define ADDRESS_IN_META(address, meta) ((void *)meta->base <= (void *)address && (void *)address <= (void *)meta->ceil)

// Allocate one object in given list
// Return: non-NULL = success
void *pfreelist_alloc(struct ARC_PFreelistMeta *meta) {	
	while (meta != NULL && meta->free_objects < 1) {
		meta = meta->next;
	}

	if (meta == NULL || meta->head == NULL) {
		ARC_DEBUG(ERR, "Found meta is NULL\n");
		return NULL;
	}

	// Get address, mark as used

	/*
	struct ARC_PFreelistNode *node = NULL;
	struct ARC_PFreelistNode **head = &meta->head;
	struct ARC_PFreelistNode **out = &node;
	ARC_ATOMIC_MFENCE;
	register struct ARC_PFreelistNode *a = meta->head;
	if (a != NULL) {
		ARC_ATOMIC_XCHG(head, &a->next, out);
	} else {
		return NULL;
	}
	*/

	spinlock_lock(&meta->lock);
	struct ARC_PFreelistNode *node = meta->head;
	if (node != NULL) {
		meta->head = meta->head->next;
		ARC_ATOMIC_DEC(meta->free_objects);
	}
	spinlock_unlock(&meta->lock);

	return (void *)node;
}

void *pfreelist_contig_alloc(struct ARC_PFreelistMeta *meta, uint64_t objects) {
	while (meta->free_objects < objects && meta != NULL) {
		meta = meta->next;
	}

	if (meta == NULL) {
		ARC_DEBUG(INFO, "Found meta is NULL\n");
		return NULL;
	}

	struct ARC_PFreelistMeta to_free = { 0 };
	to_free.object_size = meta->object_size;
	to_free.base = meta->base;
	to_free.ceil = meta->ceil;

	// Number of objects currently allocated
	uint64_t object_count = 0;
	// Limit so we don't try to allocate all of memory
	int fails = 0;
	// Object allocated from previous iteration
	void *last_allocation = NULL;
	// Base of the contiguous allocation
	void *base = NULL;
	// First allocation
	void *bottom_allocation = NULL;
	// Current allocation
	void *allocation = NULL;

	while (object_count < objects) {
		allocation = pfreelist_alloc(meta);

		if (to_free.head == NULL) {
			// Keep track of the first allocation
			// so if we fail, we are able to free
			// everything
			to_free.head = allocation;
			bottom_allocation = allocation;
			base = allocation;
		}

		if (last_allocation != NULL && abs((intptr_t)(last_allocation - allocation)) != (int64_t)meta->object_size) {
			// Keep track of this little contiguous allocation
			pfreelist_contig_free(&to_free, base, object_count + 1);

			// Move onto the next base
			base = allocation;
			fails++;
			object_count = 0;
			last_allocation = NULL;
		}

		if (fails >= 16) {
			ARC_DEBUG(ERR, "Failed more than 16 times allocating contiguous section\n");
			break;
		}

		last_allocation = allocation;
		object_count++;
	}

	meta->free_objects -= objects;

	if (fails == 0) {
		// FIRST TRY!!!!
		// Just return, no pages to be freed
		return min(base, allocation);
	}

        // Free all pages to be freed
        struct ARC_PFreelistNode *current = bottom_allocation;

	while (current->next != bottom_allocation) {
		struct ARC_PFreelistNode *next = current->next;
		pfreelist_free(meta, current);
		current = next;
	}

	return min(base, allocation);
}

// Free given address in given list
// Return: non-NULL = success
void *pfreelist_free(struct ARC_PFreelistMeta *meta, void *address) {
	if (meta == NULL || address == NULL) {
		ARC_DEBUG(ERR, "Failed to free %p in %p\n", address, meta);
		return NULL;
	}

	while (meta != NULL && !ADDRESS_IN_META(address, meta)) {
		meta = meta->next;
	}

	if (meta == NULL) {
		ARC_DEBUG(ERR, "Could not find %p in given list\n", address);
		return NULL;
	}

	struct ARC_PFreelistNode *node = (struct ARC_PFreelistNode *)address;

	// Mark as free
	spinlock_lock(&meta->lock);
	node->next = meta->head;
	meta->head = node;
	spinlock_unlock(&meta->lock);
	ARC_ATOMIC_INC(meta->free_objects);

	/*
	struct ARC_PFreelistNode *ret = NULL;
	ARC_ATOMIC_XCHG(&meta->head, &node, &ret);
	*/

	return address;
}

void *pfreelist_contig_free(struct ARC_PFreelistMeta *meta, void *address, uint64_t objects) {
	if (meta == NULL || address == NULL) {
		ARC_DEBUG(ERR, "Failed to free %p in %p\n", address, meta);
		return NULL;
	}

	while (meta != NULL && !ADDRESS_IN_META(address, meta)) {
		meta = meta->next;
	}

	if (meta == NULL) {
		ARC_DEBUG(ERR, "Could not find %p in given list\n", address);
		return NULL;
	}

	for (uint64_t i = 0; i < objects; i++) {
		struct ARC_PFreelistNode *node = (struct ARC_PFreelistNode *)(address + (i * meta->object_size));

		// Mark as free
		node->next = meta->head;
		meta->head = node;
	}

	meta->free_objects += objects;

	return address;
}

// Combine list A and list B into a single list, combined
// B is expected to be a leaf meta (meta->next == NULL)
// Return: 0 = success
// Return: -1 = object size mismatch
// Return: -2 = either list was NULL
int link_pfreelists(struct ARC_PFreelistMeta *A, struct ARC_PFreelistMeta *B) {
	if (A == NULL || B == NULL) {
		return -2;
	}

	if (A->object_size != B->object_size) {
		// Object size mismatch, should not link lists
		return -1;
	}

	// Advance to the last list
	struct ARC_PFreelistMeta *last = A;
	while (last->next != NULL) {
		last = last->next;
	}

	// Link A and B
	struct ARC_PFreelistMeta *temp = B;
	ARC_ATOMIC_XCHG(&last->next, &temp, &B->next);

	return 0;
}

struct ARC_PFreelistMeta *init_pfreelist(uint64_t _base, uint64_t _ceil, uint64_t _object_size) {
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
	int objects = (sizeof(struct ARC_PFreelistMeta) / _object_size) + 1;
	_base += objects * _object_size;
	_ceil -= _object_size;

	struct ARC_PFreelistNode *base = (struct ARC_PFreelistNode *)_base;
	struct ARC_PFreelistNode *ceil = (struct ARC_PFreelistNode *)_ceil;

	// Store meta information
	meta->base = base;
	meta->head = base;
	meta->ceil = ceil;
	meta->object_size = _object_size;
	meta->free_objects = (_ceil - _base) / _object_size - objects + 1;

	ARC_DEBUG(INFO, "Creating pfreelist from 0x%"PRIx64" (%p) to 0x%"PRIx64" (%p) with objects of %lu bytes\n", (uint64_t)_base, base, (uint64_t)_ceil, ceil, _object_size);

	// Initialize the linked list
	struct ARC_PFreelistNode *current = NULL;
	for (; _base < _ceil; _base += _object_size) {
		current = (struct ARC_PFreelistNode *)_base;
		*(uint64_t *)current = _base + _object_size;
	}

	// Set last entry to point to NULL
	*(uint64_t *)current = 0;

	return meta;
}
