/**
 * @file pslab.h
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
 * Function and structure declarations for pslab.c.
*/
#ifndef ARC_MM_ALGO_PSLAB_H
#define ARC_MM_ALGO_PSLAB_H

#include <mm/algo/pfreelist.h>
#include <stddef.h>

struct ARC_PSlab {
	/// 8 freelists for each level of the slab.
	struct ARC_PFreelist lists[8];
	/// The lowest exponent of the power of two size for the smallest object.
	int lowest_exp;
};

void *pslab_alloc(struct ARC_PSlab *meta, size_t size);
size_t pslab_free(struct ARC_PSlab *meta, void *address);
int pslab_expand(struct ARC_PSlab *meta, size_t pages_per_list);
int init_pslab(struct ARC_PSlab *meta, int lowest_exp, size_t pages_per_list);

#endif
