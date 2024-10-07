/**
 * @file pmm.h
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
#ifndef ARC_MM_PMM_H
#define ARC_MM_PMM_H

#include <global.h>

void *pmm_alloc();
void *pmm_contig_alloc(size_t objects);
void *pmm_free(void *address);
void *pmm_contig_free(void *address, size_t objects);

void *pmm_low_alloc();
void *pmm_low_contig_alloc(size_t objects);
void *pmm_low_free(void *address);
void *pmm_low_contig_free(void *address, size_t objects);

int init_pmm(struct ARC_MMap *mmap, int entries);

#endif
