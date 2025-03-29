/**
 * @file pfreelist.h
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan-MB2BSP
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
 * Abstract pfreelist implementation.
*/
#ifndef ARC_MM_ALGO_FREELIST_H
#define ARC_MM_ALGO_FREELIST_H

#include <stdint.h>
#include <lib/atomics.h>

struct ARC_PFreelistNode {
	struct ARC_PFreelistNode *next __attribute__((aligned(8)));
};

// BUG: I foresee a bug here. If an allocation is directly after
//      this header, then it is possible for this object to be overwritten
//      and screwed with, intentionally or unintentionally
struct ARC_PFreelistMeta {
	/// Current free node.
	struct ARC_PFreelistNode *head __attribute__((aligned(8)));
	/// First node.
	struct ARC_PFreelistNode *base __attribute__((aligned(8)));
	/// Last node.
	struct ARC_PFreelistNode *ceil __attribute__((aligned(8)));
	/// Size of each node in bytes.
	uint64_t object_size __attribute__((aligned(8)));
	/// Number of free objects in this meta.
	uint64_t free_objects __attribute__((aligned(8)));
	/// Lock for everything.
	ARC_GenericSpinlock lock;
}__attribute__((packed));

/**
 * Allocate a single object in the given meta.
 *
 * @param struct ARC_PFreelistMeta *meta - The list from which to allocate one object
 * @return A void * to the base of the newly allocated object.
 * */
void *pfreelist_alloc(struct ARC_PFreelistMeta *meta);

/**
 * Allocate a contiguous section of memory.
 *
 * It is not advisable to use this function if the freelist has already been used. For
 * instance, a single object has already been freed.
 *
 * @param struct ARC_PFreelistMeta *meta - The list in which to allocate the contiguous region of memory.
 * @param uint64_t objects - Number of contiguous objects to allocate.
 * @return The base address of the contiguous section.
 * */
void *pfreelist_contig_alloc(struct ARC_PFreelistMeta *meta, uint64_t objects);

/**
 * Free the object at the given address in the given meta.
 *
 * @param struct ARC_PFreelistMeta *meta - The pfreelist in which to free the address
 * @param void *address - A pointer to the base of the given object to be freed.
 * @return /a address when successfull.
 * */
void *pfreelist_free(struct ARC_PFreelistMeta *meta, void *address);

/**
 * Free a contiguous section of memory.
 *
 * It is not advisable to use this function if the freelist has already been used. For
 * instance, a single object has already been freed.
 *
 * @param struct ARC_PFreelistMeta *meta - The list in which to free the contiguous region of memory.
 * @param void *address - The base address of the contiguous section.
 * @param uint64_t objects - The number of objects the section consists of.
 * @return The base address if the free was successful. */
void *pfreelist_contig_free(struct ARC_PFreelistMeta *meta, void *address, uint64_t objects);

/**
 * Initialize the given memory as a pfreelist.
 *
 * @param uint64_t _base - The lowest address within the list.
 * @param uint64_t _ceil - The highest address within the list + object_size.
 * @param uint64_t _object_size - The size of each object in bytes.
 * @return returns the pointer to the pfreelist meta (_base == return value).
 * */
struct ARC_PFreelistMeta *init_pfreelist(uint64_t _base, uint64_t _ceil, uint64_t _object_size);

#endif
