/**
 * @file pmm.h
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
#ifndef ARC_MM_PMM_H
#define ARC_MM_PMM_H

#include <global.h>

// These functions return virtual addresses, but ARC_HHDM_TO_PHYS can be used on them
// to convert them to physical addresses

// These functions use the freelists
void *pmm_alloc_page();
void *pmm_free_page(void *address);
void *pmm_alloc_pages(size_t pages);
void *pmm_free_pages(void *address, size_t pages);

void *pmm_low_alloc_page();
void *pmm_low_free_page(void *address);
void *pmm_low_alloc_pages(size_t pages);
void *pmm_low_free_pages(void *address, size_t pages);

// These functions use the vbuddies
void *pmm_alloc(size_t size);
size_t pmm_free(void *address);

int init_pmm_contig();
int init_pmm(struct ARC_BootMMap *mmap, int entries);

#endif
