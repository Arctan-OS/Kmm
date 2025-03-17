/**
 * @file pbuddy.c
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

#include <mm/algo/pbuddy.h>
#include <mm/algo/allocator.h>
#include <mm/pmm.h>
#include <global.h>
#include <stdbool.h>
#include <lib/util.h>

struct pbuddy_node {
	struct buddy_node *next;
	size_t size;
};

/*
static int split(struct ARC_PBuddyMeta *meta, struct buddy_node *node) {
	(void)meta;
	(void)node;
	return 0;
}

static int merge(struct buddy_node *base) {
	(void)base;
	
	return 0;
}
*/

void *pbuddy_alloc(struct ARC_PBuddyMeta *meta, size_t size) {
	(void)meta;
	(void)size;

	return NULL;
}       

size_t pbuddy_free(struct ARC_PBuddyMeta *meta, void *address) {
	(void)meta;
	(void)address;

	return 0;
}

int init_pbuddy(struct ARC_PBuddyMeta *meta, void *base, size_t size, size_t smallest_object) {
        ARC_DEBUG(INFO, "Initializing new buddy allocator (%lu bytes, lowest %lu bytes) at %p\n", size, smallest_object, base);

	meta->base = base;
	meta->ceil = base + size;
	meta->smallest_object = smallest_object;

	init_static_mutex(&meta->mutex);

	struct pbuddy_node *head = (struct pbuddy_node *)ialloc(sizeof(*head));

	if (head == NULL) {
		return -1;
	}

	memset(head, 0, sizeof(*head));

	head->size = size;
	head->next = NULL;

	meta->tree = head;

        return 0;
}
