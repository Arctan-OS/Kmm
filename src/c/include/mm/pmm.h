/**
 * @file pmm.h
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
 * Function declarations for pmm.c.
*/
#ifndef ARC_MM_PMM_H
#define ARC_MM_PMM_H

#include <global.h>

void *pmm_alloc(size_t size);
size_t pmm_free(void *address);

void *pmm_low_alloc(size_t size);
size_t pmm_low_free(void *address);

size_t pmm_alloc_fast_pages(size_t count, bool low);

void *pmm_fast_page_alloc();
size_t pmm_fast_page_free(void *address);

void *pmm_fast_page_alloc_low();
size_t pmm_fast_page_free_low(void *address);

int init_pmm(struct ARC_MMap *mmap, int entries);

#endif
