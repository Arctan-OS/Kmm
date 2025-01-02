/**
 * @file freelist.c
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
 * Abstract freelist implementation.
*/
#include <mm/algo/freelist.h>
#include <lib/atomics.h>
#include <lib/util.h>
#include <global.h>
#include <stdint.h>
#include <inttypes.h>

#define ADDRESS_IN_META(address, meta) ((void *)meta->base <= (void *)address && (void *)address <= (void *)meta->ceil)

// Allocate one object in given list
// Return: non-NULL = success
void *freelist_alloc(struct ARC_FreelistMeta *meta) {
	mutex_lock(&meta->mutex);

	while (meta != NULL && meta->free_objects < 1) {
		if (meta->next != NULL) {
			mutex_lock(&meta->next->mutex);
		}


		mutex_unlock(&meta->mutex);
		meta = meta->next;
	}

	if (meta == NULL) {
		ARC_DEBUG(ERR, "Found meta is NULL\n");
		return NULL;
	}

	// Get address, mark as used
	void *address = (void *)meta->head;
	meta->head = meta->head->next;

	meta->free_objects--;

	mutex_unlock(&meta->mutex);

	return address;
}

void *freelist_contig_alloc(struct ARC_FreelistMeta *meta, uint64_t objects) {
	while (meta->free_objects < objects && meta != NULL) {
		meta = meta->next;
	}

	if (meta == NULL) {
		ARC_DEBUG(INFO, "Found meta is NULL\n");
		return NULL;
	}

	struct ARC_FreelistMeta to_free = { 0 };
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
		allocation = freelist_alloc(meta);

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
			freelist_contig_free(&to_free, base, object_count + 1);

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
        struct ARC_FreelistNode *current = bottom_allocation;

	while (current->next != bottom_allocation) {
		struct ARC_FreelistNode *next = current->next;
		freelist_free(meta, current);
		current = next;
	}

	return min(base, allocation);
}

// Free given address in given list
// Return: non-NULL = success
void *freelist_free(struct ARC_FreelistMeta *meta, void *address) {
	if (meta == NULL || address == NULL) {
		ARC_DEBUG(ERR, "Failed to free %p in %p\n", address, meta);
		return NULL;
	}

	mutex_lock(&meta->mutex);

	while (meta != NULL && !ADDRESS_IN_META(address, meta)) {
		if (meta->next != NULL) {
			mutex_lock(&meta->next->mutex);
		}

		mutex_unlock(&meta->mutex);
		meta = meta->next;
	}

	if (meta == NULL) {
		ARC_DEBUG(ERR, "Could not find %p in given list\n", address);
		return NULL;
	}

	struct ARC_FreelistNode *node = (struct ARC_FreelistNode *)address;

	// Mark as free
	node->next = meta->head;
	meta->head = node;

	meta->free_objects++;

	mutex_unlock(&meta->mutex);

	return address;
}

void *freelist_contig_free(struct ARC_FreelistMeta *meta, void *address, uint64_t objects) {
	if (meta == NULL || address == NULL) {
		ARC_DEBUG(ERR, "Failed to free %p in %p\n", address, meta);
		return NULL;
	}

	mutex_lock(&meta->mutex);

	while (meta != NULL && !ADDRESS_IN_META(address, meta)) {
		if (meta->next != NULL) {
			mutex_lock(&meta->next->mutex);
		}

		mutex_unlock(&meta->mutex);
		meta = meta->next;
	}

	if (meta == NULL) {
		ARC_DEBUG(ERR, "Could not find %p in given list\n", address);
		return NULL;
	}

	for (uint64_t i = 0; i < objects; i++) {
		struct ARC_FreelistNode *node = (struct ARC_FreelistNode *)(address + (i * meta->object_size));

		// Mark as free
		node->next = meta->head;
		meta->head = node;
	}

	meta->free_objects += objects;

	mutex_unlock(&meta->mutex);

	return address;
}

// Combine list A and list B into a single list, combined
// Return: 0 = success
// Return: -1 = object size mismatch
// Return: -2 = either list was NULL
int link_freelists(struct ARC_FreelistMeta *A, struct ARC_FreelistMeta *B) {
	if (A == NULL || B == NULL) {
		return -2;
	}

	if (A->object_size != B->object_size) {
		// Object size mismatch, should not link lists
		return -1;
	}

	mutex_lock(&A->mutex);

	// Advance to the last list
	struct ARC_FreelistMeta *last = A;
	while (last->next != NULL) {
		mutex_lock(&last->next->mutex);
		mutex_unlock(&last->mutex);
		last = last->next;
	}

	// Link A and B
	last->next = B;

	mutex_unlock(&last->mutex);

	return 0;
}

struct ARC_FreelistMeta *init_freelist(uint64_t _base, uint64_t _ceil, uint64_t _object_size) {
	if (_base > _ceil || _object_size == 0) {
		// Invalid parameters
		return NULL;
	}

	if (_ceil - _base < _object_size + sizeof(struct ARC_FreelistMeta)) {
		// There is not enough space for one object
		return NULL;
	}

	struct ARC_FreelistMeta *meta = (struct ARC_FreelistMeta *)_base;

	memset(meta, 0, sizeof(struct ARC_FreelistMeta));

	init_static_mutex(&meta->mutex);

	// Number of objects to accomodate meta
	int objects = (sizeof(struct ARC_FreelistMeta) / _object_size) + 1;
	_base += objects * _object_size;
	_ceil -= _object_size;

	struct ARC_FreelistNode *base = (struct ARC_FreelistNode *)_base;
	struct ARC_FreelistNode *ceil = (struct ARC_FreelistNode *)_ceil;

	// Store meta information
	meta->base = base;
	meta->head = base;
	meta->ceil = ceil;
	meta->object_size = _object_size;
	meta->free_objects = (_ceil - _base) / _object_size - objects + 1;

	ARC_DEBUG(INFO, "Creating freelist from 0x%"PRIx64" (%p) to 0x%"PRIx64" (%p) with objects of %lu bytes\n", (uint64_t)_base, base, (uint64_t)_ceil, ceil, _object_size);

	// Initialize the linked list
	struct ARC_FreelistNode *current = NULL;
	for (; _base < _ceil; _base += _object_size) {
		current = (struct ARC_FreelistNode *)_base;
		*(uint64_t *)current = _base + _object_size;
	}

	// Set last entry to point to NULL
	*(uint64_t *)current = 0;

	return meta;
}
