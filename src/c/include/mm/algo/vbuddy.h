/**
 * @file buddy.h
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
#ifndef ARC_MM_ALGO_VBUDDY_H
#define ARC_MM_ALGO_VBUDDY_H

#include <stddef.h>
#include <lib/atomics.h>
#include <mm/algo/pfreelist.h>

struct ARC_VBuddyMeta {
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

void *vbuddy_alloc(struct ARC_VBuddyMeta *meta, size_t size);
size_t vbuddy_free(struct ARC_VBuddyMeta *meta, void *address);

/**
 * Create a buddy allocator
 *
 * @param struct ARC_VBuddyMeta *meta - Meta of the allocator.
 * @param void *base - First allocatable address.
 * @param size_t size - Size of the first allocatable region (ensure this is aligned to the nearest power of 2).
 * @param size_t smallest_object - Size of the smallest allocatable object (ensure this is aligned to the nearest power of 2).
 * @return zero upon success.
 *  */
int init_vbuddy(struct ARC_VBuddyMeta *meta, void *base, size_t size, size_t smallest_object);

#endif
