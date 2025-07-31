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
 * Implementation of a watermark memory management algorithm for non-present,
 * "virtual", memory regions. The algorithm keeps track of free regions by
 * using dynamically allocated metadata structures. A suitably sized free
 * region is then allocated from. Free regions are greedily merged on every
 * free.
 *
 * NOTE: Due to the dynamic metdata structures that are allocated using the
 *       general purpose kernel allocator, this algorithm may not be used
 *       in the PMM or general purpose kernel allocator.
 *
 * NOTE: Currently very large locks are used to ensure synchornization in the
 *       allocated and free region lists. This should be refined so that allocations
 *       and frees do not take long.
*/
#include <mm/allocator.h>
#include <mm/algo/vwatermark.h>
#include <global.h>

#define ADDRESS_IN_META(address, meta) ((void *)meta->base <= (void *)address && (void *)address <= (void *)(meta->base + meta->size))

void *vwatermark_alloc(struct ARC_VWatermark *list, size_t size) {
        if (list == NULL  || list->head == NULL || size == 0) {
                return NULL;
        }

        struct ARC_VWatermarkNode *node = alloc(sizeof(*node));

        if (node == NULL) {
                return NULL;
        }

        spinlock_lock(&list->order_lock);

        struct ARC_VWatermarkMeta *current = list->head;
        while (current != NULL && current->free == NULL) {
                current = current->next;
        }

        spinlock_unlock(&list->order_lock);

        if (current == NULL) {
                free(node);
                return NULL;
        }

        spinlock_lock(&current->free_lock);

        struct ARC_VWatermarkNode *free_node = current->free;
        struct ARC_VWatermarkNode *prev = NULL;

        while (free_node != NULL && free_node->ceil - free_node->base < size) {
                prev = free_node;
                free_node = free_node->next;
        }

        if (free_node == NULL) {
                spinlock_unlock(&current->free_lock);
                free(node);
                return NULL;
        }

        printf("Found free node: %p\n", free_node);

        void *a = (void *)free_node->base;

        bool used_up = (free_node->base + size == free_node->ceil);

        ARC_DEBUG(INFO, "%"PRIx64" + %lu == %"PRIx64 "=> %d\n", free_node->base, size, free_node->ceil, used_up);

        if (used_up) {
                if (prev != NULL) {
                        prev->next = free_node->next;
                } else {
                        current->free = free_node->next;
                }
        } else {
                free_node->base += size;
        }

        spinlock_unlock(&current->free_lock);

        if (used_up) {
                free(node);
                node = free_node;
        } else {
                node->base = (uintptr_t)a;
                node->ceil = node->base + size;
        }

        spinlock_lock(&current->allocated_lock);
        node->next = current->allocated;
        current->allocated = node;
        spinlock_unlock(&current->allocated_lock);

        return a;
}

static int vwatermark_attempt_merge(struct ARC_VWatermarkNode *list) {
        struct ARC_VWatermarkNode *current = list;
        struct ARC_VWatermarkNode *next = current->next;

        while (current != NULL && next != NULL) {
                if (next->base == current->ceil) {
                        current->ceil = next->ceil;
                        current->next = next->next;
                        free(next);
                } else if (next->ceil == current->base) {
                        current->base = next->base;
                        current->next = next->next;
                        free(next);
                }

                current = current->next;
                if (current != NULL) {
                        next = current->next;
                }
        }

        return 0;
}

size_t vwatermark_free(struct ARC_VWatermark *list, void *address) {
        if (list == NULL || address == NULL) {
                return 0;
        }

        spinlock_lock(&list->order_lock);

        struct ARC_VWatermarkMeta *current = list->head;
        while (current != NULL && !ADDRESS_IN_META(address, current)) {
                current = current->next;
        }

        spinlock_unlock(&list->order_lock);

        if (current == NULL) {
                ARC_DEBUG(ERR, "Could not find meta address belongs to\n");
                return 0;
        }

        spinlock_lock(&current->allocated_lock);

        struct ARC_VWatermarkNode *allocated = current->allocated;
        struct ARC_VWatermarkNode *prev = NULL;

        while (allocated != NULL && allocated->base != (uintptr_t)address) {
                prev = allocated;
                allocated = allocated->next;
        }

        if (allocated == NULL) {
                ARC_DEBUG(ERR, "Could not find %p in meta\n")
                spinlock_unlock(&current->allocated_lock);
                return 0;
        }

        if (prev != NULL) {
                prev->next = allocated->next;
        } else {
                current->allocated = allocated->next;
        }

        spinlock_unlock(&current->allocated_lock);

        size_t size = allocated->ceil - allocated->base;

        spinlock_lock(&current->free_lock);
        allocated->next = current->free;
        current->free = allocated;
        vwatermark_attempt_merge(current->free);
        spinlock_unlock(&current->free_lock);

        return size;
}

int init_vwatermark(struct ARC_VWatermark *list, struct ARC_VWatermarkMeta *meta, uintptr_t base, size_t len) {
        if (meta == NULL || base == 0 || len == 0) {
                ARC_DEBUG(ERR, "No meta provided, allocator at NULL, or of zero length\n");
                return -1;
        }

        struct ARC_VWatermarkNode *free = alloc(sizeof(*free));

        if (free == NULL) {
                return -2;
        }

        free->base = base;
        free->ceil = base + len;
        free->next = NULL;

        meta->free = free;
        meta->allocated = NULL;

        meta->base = base;
        meta->size = len;

        init_static_spinlock(&meta->allocated_lock);
        init_static_spinlock(&meta->free_lock);

        spinlock_lock(&list->order_lock);
        meta->next = list->head;
        list->head = meta;
        spinlock_unlock(&list->order_lock);

        ARC_DEBUG(INFO, "Initialized vwatermark allocator at 0x%"PRIx64" to 0x%"PRIx64"\n", meta->base, meta->base + meta->size);

        return 0;
}
