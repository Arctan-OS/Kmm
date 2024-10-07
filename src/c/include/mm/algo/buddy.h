/**
 * @file buddy.h
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2024 awewsomegamer
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
#ifndef ARC_MM_ALGO_BUDDY_H
#define ARC_MM_ALGO_BUDDY_H

#include <stddef.h>
#include <lib/atomics.h>
#include <mm/algo/freelist.h>

struct ARC_BuddyMeta {
	/// Base of the allocator.
	void *base;
	/// Ceiling of the allocator.
	void *ceil;
	/// Allocator tree.
	void *tree;
	/// The size of the smallest object that can be allocated.
	size_t smallest_object;
	/// Lock for the meta.
	ARC_GenericMutex mutex;
};

void *buddy_alloc(struct ARC_BuddyMeta *meta, size_t size);
size_t buddy_free(struct ARC_BuddyMeta *meta, void *address);

/**
 * Create a buddy allocator
 *
 * @param struct ARC_BuddyMeta *meta - Meta of the allocator.
 * @param void *base - First allocatable address.
 * @param size_t size - Size of the first allocatable region (ensure this is aligned to the nearest power of 2).
 * @param size_t smallest_object - Size of the smallest allocatable object (ensure this is aligned to the nearest power of 2).
 * @return zero upon success.
 *  */
int init_buddy(struct ARC_BuddyMeta *meta, void *base, size_t size, size_t smallest_object);

#endif
