/**
 * @file vwatermark.c
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
 * Function and structure declarations for vwatermark.c.
*/
#ifndef ARC_MM_ALGO_VWATERMARK_H
#define ARC_MM_ALGO_VWATERMARK_H

#include <lib/atomics.h>
#include <stddef.h>
#include <stdint.h>

struct ARC_VWatermarkNode {
        struct ARC_VWatermarkNode *next;
        uintptr_t base;
        uintptr_t ceil;
};

struct ARC_VWatermarkMeta {
        struct ARC_VWatermarkMeta *next;
        struct ARC_VWatermarkNode *allocated;
        struct ARC_VWatermarkNode *free;
        uintptr_t base;
        size_t size;
        ARC_GenericSpinlock allocated_lock;
        ARC_GenericSpinlock free_lock;
};

struct ARC_VWatermark {
        struct ARC_VWatermarkMeta *head;
        ARC_GenericSpinlock order_lock;
};

void *vwatermark_alloc(struct ARC_VWatermark *list, size_t size);
size_t vwatermark_free(struct ARC_VWatermark *list, void *address);
int init_vwatermark(struct ARC_VWatermark *list, struct ARC_VWatermarkMeta *meta, uintptr_t base, size_t len);

#endif
