/**
 * @file pwatermark.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan-OS/Kmm - Operating System Kernel Memory Manager
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan-OS/Kmm.
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
 * Implementation of functions to initialize and allocate from a region of
 * present memory using a basic watermark allocator. This allocator is useful
 * for very early allocations where freeing is not needed.
*/
#include <mm/algo/pwatermark.h>
#include <global.h>

void *pwatermark_alloc(struct ARC_PWatermark *list, size_t size) {
        if (list == NULL  || list->head == NULL || size == 0) {
                return NULL;
        }

        spinlock_lock(&list->order_lock);

        struct ARC_PWatermarkMeta *current = list->head;
        while (current != NULL && current->base + current->off + size > current->ceil) {
                current = current->next;
        }

        spinlock_unlock(&list->order_lock);

        if (current == NULL) {
                return NULL;
        }

        // TODO: Synchronization
        void *a = (void *)(current->base + current->off);
        current->off += size;

        return a;
}

int init_pwatermark(struct ARC_PWatermark *list, uintptr_t base, size_t len) {
        if (list == NULL || base == 0 || len <= sizeof(struct ARC_PWatermarkMeta)) {
                return -1;
        }

        struct ARC_PWatermarkMeta *meta = (struct ARC_PWatermarkMeta *)base;

        meta->base = base + sizeof(*meta);
        meta->ceil = meta->base + len - sizeof(*meta);
        meta->off = 0;

        ARC_ATOMIC_XCHG(&list->head, &meta, &meta->next);

        ARC_DEBUG(INFO, "Initialized pwatermark allocator at 0x%"PRIx64" to 0x%"PRIx64"\n", meta->base, meta->ceil);

        return 0;
}
