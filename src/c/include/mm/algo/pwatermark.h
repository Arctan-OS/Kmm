/**
 * @file pwatermark.h
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
 * Function and structure declarations for pwatermark.c.
*/
#ifndef ARC_MM_ALGO_PWATERMARK_H
#define ARC_MM_ALGO_PWATERMARK_H

#include "lib/atomics.h"
#include <stddef.h>
#include <stdint.h>

struct ARC_PWatermarkMeta {
        struct ARC_PWatermarkMeta *next;
        uintptr_t base;
        uintptr_t ceil;
        size_t off;
};

struct ARC_PWatermark {
        struct ARC_PWatermarkMeta *head;
        ARC_GenericSpinlock order_lock;
};

void *pwatermark_alloc(struct ARC_PWatermark *list, size_t size);
int init_pwatermark(struct ARC_PWatermark *list, uintptr_t base, size_t len);

#endif
