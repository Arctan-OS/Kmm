/**
 * @file pbuddy.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan-OS/Kernel - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan-MB2BSP
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
 * Abstract pfreelist implementation.
*/
#include "lib/atomics.h"
#include <arch/info.h>
#include <lib/util.h>
#include <mm/pmm.h>
#include <mm/algo/pbuddy.h>
#include <global.h>

void *pbuddy_alloc(struct ARC_PBuddy *list, size_t size) {
        if (list == NULL || list->head == NULL || size < PMM_LOWEST_BUDDY_SIZE) {
                return NULL;
        }

        spinlock_lock(&list->order_lock);

        struct ARC_PBuddyMeta *current = list->head;
        struct ARC_PBuddyMeta *last = NULL;

        while (current != NULL && current->free_objects == 0) {
                last = current;
                current = current->next;
        }

        if (last != NULL) {
                last->next = list->head;
                void *tmp = list->head->next;
                list->head->next = current->next;
                current->next = tmp;
                list->head = current;
        }

        spinlock_unlock(&list->order_lock);

        SIZE_T_NEXT_POW2(size);

        uint32_t exp = __builtin_ctz(size);

        // TODO: Implement

        return NULL;
}

size_t pbuddy_free(struct ARC_PBuddy *list, void *address) {
        if (list == NULL || address == NULL) {
                return 0;
        }

        // TODO: Implement

        return 0;
}

int pbuddy_remove(struct ARC_PBuddy *list, struct ARC_PBuddyMeta *meta) {
        return -1;
}

static int pbuddy_get_new_meta(struct ARC_PBuddy *list, struct ARC_PBuddyMeta **meta_out, size_t obj_size) {
        if (list == NULL || meta_out == NULL) {
                return -1;
        }

        retry:;

        *meta_out = pfreelist_alloc(&list->meta_tables);

        if (*meta_out != NULL) {
                memset(*meta_out, 0, obj_size);
                return 0;
        }

        uintptr_t base = (uintptr_t)pmm_fast_page_alloc();


        if (init_pfreelist(&list->meta_tables, base, base + PAGE_SIZE, obj_size) != 0) {
                return -2;
        }

        goto retry;
}

int init_pbuddy(struct ARC_PBuddy *list, uintptr_t _base, uint32_t full_exp) {
        if (list == NULL || _base == 0 || full_exp < SMALLEST_PAGE_SIZE_EXPONENT) {
                ARC_DEBUG(ERR, "Failed to initialize buddy allocator, improper parameters\n");
                return -1;
        }

        struct ARC_PBuddyMeta *meta = NULL;
        uint32_t ptr_count = (full_exp - PMM_LOWEST_BUDDY_EXPONENT) + 1;
        size_t obj_size = sizeof(*meta) + ptr_count * sizeof(void *);

        if (pbuddy_get_new_meta(list, &meta, obj_size) != 0) {
                ARC_DEBUG(ERR, "Failed to get new meta\n");
                return -2;
        }

        meta->full_exp = full_exp;
        meta->base = _base;

        ARC_ATOMIC_XCHG(&list->head, &meta, &meta->next);

        return 0;
}
