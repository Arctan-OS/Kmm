/**
 * @file pfreelist.h
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
 * Function and structure declarations for pfreelist.c.
*/
#ifndef ARC_MM_ALGO_PFREELIST_H
#define ARC_MM_ALGO_PFREELIST_H

#include <lib/atomics.h>
#include <stdint.h>
#include <stddef.h>

struct ARC_PFreelistNode {
	struct ARC_PFreelistNode *next;
};

// BUG: I foresee a bug here. If an allocation is directly after
//      this header, then it is possible for this object to be overwritten
//      and screwed with, intentionally or unintentionally
struct ARC_PFreelistMeta {
	struct ARC_PFreelistMeta *next;
	struct ARC_PFreelistNode *head;
	/// First node.
	struct ARC_PFreelistNode *base;
	/// Last node.
	struct ARC_PFreelistNode *ceil;
	size_t free_objects;
	/// Lock for everything.
	ARC_GenericSpinlock lock;
};

struct ARC_PFreelist {
	struct ARC_PFreelistMeta *head;
	ARC_GenericSpinlock ordering_lock;
};

void *pfreelist_alloc(struct ARC_PFreelist *list);
void *pfreelist_free(struct ARC_PFreelist *list, void *address);

/**
 * Initializes a new pfreelist meta into the given list
 *
 * @param struct ARC_PFreelist *list - The list to initialize the region into.
 * @param uintptr_t _base - The start of the region (address of the first node).
 * @param uintptr_t _ceil - The end of the region (address of the last node + _object_size)
 * @param size_t _object_size - The size of each object in the list.
 * @return zero on success.
 * */
int init_pfreelist(struct ARC_PFreelist *list, uintptr_t _base, uintptr_t _ceil, size_t _object_size);

#endif
