/**
 * @file allocator.h
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
 * Header file containing functions for the allocation of data structures for
 * the various memory management algorithms.
*/
#ifndef ARC_MM_IALLOCATOR
#define ARC_MM_IALLOCATOR

#include <stddef.h>

void *ialloc(size_t size);
void *icalloc(size_t size, size_t count);
size_t ifree(void *address);

int iallocator_expand(size_t pages);

int init_iallocator(size_t pages);

#endif
