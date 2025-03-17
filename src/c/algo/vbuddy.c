/**
 * @file buddy.c
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

#include <mm/algo/vbuddy.h>
#include <mm/algo/allocator.h>
#include <mm/pmm.h>
#include <global.h>
#include <stdbool.h>
#include <lib/util.h>

struct vbuddy_node {
	struct vbuddy_node *next;
	void *base;
	size_t size;
	uint32_t attributes; // Bit | Description
			     // 0   | 1: Allocated, 0: Free
};

static int split(struct ARC_VBuddyMeta *meta, struct vbuddy_node *node) {
	if (node == NULL || (node->attributes & 1) == 1) {
		return -1;
	}

	if (node->size == meta->smallest_object) {
		return -2;
	}

	size_t new_size = node->size >> 1;

	struct vbuddy_node *buddy = ialloc(sizeof(*buddy));

	if (buddy == NULL) {
		return -3;
	}

	memset(buddy, 0, sizeof(*buddy));

	buddy->size = new_size;
	buddy->base = node->base + new_size;
	buddy->next = node->next;

	node->next = buddy;
	node->size = new_size;

	return 0;
}

static int merge(struct vbuddy_node *base) {
	if (base == NULL || (base->attributes & 1) == 1) {
		return -1;
	}

	if (base->next == NULL || (base->next->attributes & 1) == 1 || base->size != base->next->size) {
		return -2;
	}

	base->size <<= 1;
	void *tmp = base->next;
	base->next = base->next->next;
	ifree(tmp);

	return 0;
}

void *vbuddy_alloc(struct ARC_VBuddyMeta *meta, size_t size) {
	if (meta == NULL || size == 0 || meta->tree == NULL) {
		ARC_DEBUG(ERR, "Invalid parameters\n");
		return NULL;
	}

	if (size < meta->smallest_object) {
		ARC_DEBUG(ERR, "Size to allocate is below limit\n");
		return NULL;
	}

	struct vbuddy_node *current = meta->tree;

	// Align the size up
	SIZE_T_NEXT_POW2(size);

	while (current != NULL) {
		if ((current->attributes & 1) == 1) {
			// Already allocated
			goto cycle_end;
		}

		if (size > current->size) {
			goto cycle_end;
		}

		if (current->size > size) {
			while (current->size != size) {
				if (split(meta, current) != 0) {
					break;
				}
			}
		}

		if (current->size == size) {
			break;
		}

		cycle_end:
		current = current->next;
	}

	if (current == NULL) {
		return NULL;
	}

	current->attributes |= 1;

        return current->base;
}

size_t vbuddy_free(struct ARC_VBuddyMeta *meta, void *address) {
	if (meta == NULL || meta->tree == NULL) {
		return 0;
	}

	struct vbuddy_node *current = meta->tree;

	while (current != NULL && current->base != address) {
		current = current->next;
	}

	if (current == NULL || (current->attributes & 1) == 0) {
		return 0;
	}

	current->attributes &= ~1;

	size_t ret = current->size;

	merge(current);

        return ret;
}

int init_vbuddy(struct ARC_VBuddyMeta *meta, void *base, size_t size, size_t smallest_object) {
        ARC_DEBUG(INFO, "Initializing new vbuddy allocator (%lu bytes, lowest %lu bytes) at %p\n", size, smallest_object, base);

	meta->base = base;
	meta->ceil = base + size;
	meta->smallest_object = smallest_object;

	init_static_mutex(&meta->mutex);

	struct vbuddy_node *head = (struct vbuddy_node *)ialloc(sizeof(*head));

	if (head == NULL) {
		return -1;
	}

	memset(head, 0, sizeof(*head));

	head->base = base;
	head->size = size;
	head->next = NULL;
	head->attributes = 0;

	meta->tree = head;

        return 0;
}
