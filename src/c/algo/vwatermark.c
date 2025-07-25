/**
 * @file vwatermark.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan-OS/Kernel - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
 *
 * This file is part of Arctan-OS/Kernel.
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
#include <mm/algo/vwatermark.h>
#include <global.h>

void *vwatermark_alloc(struct ARC_VWatermark *list, size_t size) {
        if (list == NULL  || list->head == NULL || size == 0) {
                return NULL;
        }

        spinlock_lock(&list->order_lock);

        struct ARC_VWatermarkMeta *current = list->head;
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

int init_vwatermark(struct ARC_VWatermark *list, struct ARC_VWatermarkMeta *meta, uintptr_t base, size_t len) {
        if (meta == NULL || base == 0 || len == 0) {
                ARC_DEBUG(ERR, "No meta provided, allocator at NULL, or of zero length\n");
                return -1;
        }

        meta->base = base;
        meta->ceil = base + len;
        meta->off = 0;

        ARC_ATOMIC_XCHG(&list->head, &meta, &meta->next);

        ARC_DEBUG(INFO, "Initialized vwatermark allocator at 0x%"PRIx64" to 0x%"PRIx64"\n", meta->base, meta->ceil);

        return 0;
}
