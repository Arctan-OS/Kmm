/**
 * @file vmm.h
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
 * Function and structure declarations for vmm.c.
*/
#ifndef ARC_MM_VMM_H
#define ARC_MM_VMM_H

#include "mm/algo/vwatermark.h"
#include <stddef.h>

typedef struct ARC_VMMMeta {
        struct ARC_VWatermark vwatermark;
} ARC_VMMMeta;

void *vmm_alloc(ARC_VMMMeta *meta, size_t size);
size_t vmm_free(ARC_VMMMeta *meta, void *address);
ARC_VMMMeta *init_vmm(void *base, size_t size);

#endif
