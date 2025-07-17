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
*/
#include "arch/x86-64/config.h"
#include <lib/atomics.h>
#include <arch/info.h>
#include <lib/util.h>
#include <mm/pmm.h>
#include <mm/algo/pbuddy.h>
#include <global.h>

#define ADDRESS_IN_META(address, meta, exp) ((void *)meta->base <= (void *)address && (void *)address <= (void *)(meta->base + (1 << exp)))

typedef uint32_t idx_t;

#define META_NULL (idx_t)-1

static int pbuddy_get_new_meta(struct ARC_PBuddy *list, struct ARC_PBuddyMeta **meta_out, size_t obj_size) {
        if (list == NULL || meta_out == NULL) {
                return -1;
        }

        retry:;

        *meta_out = pfreelist_alloc(&list->metas);

        if (*meta_out != NULL) {
                memset(*meta_out, 0, obj_size);
                return 0;
        }

        uintptr_t base = (uintptr_t)pmm_fast_page_alloc();

        if (init_pfreelist(&list->metas, base, base + PAGE_SIZE, obj_size) != 0) {
                return -2;
        }

        goto retry;
}

static idx_t pbuddy_ptr2idx(struct ARC_PBuddyMeta *meta, uintptr_t ptr) {
        if (ptr == 0) {
                return META_NULL;
        }

        ptr -= meta->base;
        ptr >>= PMM_BUDDY_LOWEST_EXPONENT;
        ptr /= sizeof(struct ARC_PBuddyNodeMeta);

        return (idx_t)ptr;
}

static void *pbuddy_merge(struct ARC_PBuddyMeta *meta, struct ARC_PBuddyNode *node) {
        idx_t idx = pbuddy_ptr2idx(meta, (uintptr_t)node);

        int exp = meta->node_metas[idx].exp;

        if (exp >= meta->exp) {
                return NULL;
        }

        int exp_idx = exp - PMM_BUDDY_LOWEST_EXPONENT;

        uintptr_t _buddy = (uintptr_t)node ^ (1 << exp);
        struct ARC_PBuddyNode *buddy = (struct ARC_PBuddyNode *)_buddy;

        if (buddy->canary_high != ARC_PBUDDY_CANARY_HIGH
            || buddy->canary_low != ARC_PBUDDY_CANARY_LOW) {
                ARC_DEBUG(ERR, "Failed to merge, buddy has improper canaries\n");
                return NULL;
        }

        struct ARC_PBuddyNode *primary = min(node, buddy);
        struct ARC_PBuddyNode *secondary = max(node, buddy);

        secondary->canary_high = 0x0;
        secondary->canary_low = 0x0;

        struct ARC_PBuddyNode *current = meta->free[exp_idx];
        struct ARC_PBuddyNode *last = NULL;
        while (current != NULL && current != buddy) {
                last = current;
                current = current->next;
        }

        if (current == NULL) {
                ARC_DEBUG(ERR, "Could not remove buddy from freelist\n");
                return NULL;
        }

        if (last != NULL) {
                last->next = current->next;
        } else {
                meta->free[exp_idx] = current->next;
        }

        exp = ++meta->node_metas[idx].exp;
        exp_idx++;

        return primary;
}

static size_t pbuddy_release(struct ARC_PBuddyMeta *meta, struct ARC_PBuddyNode *node) {
        idx_t idx = pbuddy_ptr2idx(meta, (uintptr_t)node);

        int exp = meta->node_metas[idx].exp;
        size_t size = 1 << exp;

        struct ARC_PBuddyNode *t = node;
        struct ARC_PBuddyNode *t1 = NULL;

        while ((t1 = pbuddy_merge(meta, t)) != NULL) {
                t = t1;
        }

        exp = meta->node_metas[idx].exp;
        int exp_idx = exp - PMM_BUDDY_LOWEST_EXPONENT;

        node->canary_high = ARC_PBUDDY_CANARY_HIGH;
        node->canary_low = ARC_PBUDDY_CANARY_LOW;

        ARC_ATOMIC_XCHG(&meta->free[exp_idx], &t, &t->next);

        return size;
}

static int pbuddy_split(struct ARC_PBuddyMeta *meta, struct ARC_PBuddyNode *node) {
        idx_t idx = pbuddy_ptr2idx(meta, (uintptr_t)node);
        int exp = meta->node_metas[idx].exp;
        int exp_idx = exp - PMM_BUDDY_LOWEST_EXPONENT;

        if (exp <= PMM_BUDDY_LOWEST_EXPONENT) {
                ARC_DEBUG(ERR, "Exponent below minimum\n");
                return -2;
        }

        exp = --meta->node_metas[idx].exp;
        exp_idx--;

        uintptr_t _buddy = (uintptr_t)node ^ (1 << exp);
        struct ARC_PBuddyNode *buddy = (struct ARC_PBuddyNode *)_buddy;

        buddy->canary_high = ARC_PBUDDY_CANARY_HIGH;
        buddy->canary_low = ARC_PBUDDY_CANARY_LOW;

        idx_t buddy_idx = pbuddy_ptr2idx(meta, (uintptr_t)buddy);
        meta->node_metas[buddy_idx].exp = exp;

        ARC_ATOMIC_XCHG(&meta->free[exp_idx], &buddy, &buddy->next);

        return 0;
}

static void *pbuddy_acquire(struct ARC_PBuddyMeta *meta, int exp) {
        int exp_idx = exp - PMM_BUDDY_LOWEST_EXPONENT;
        struct ARC_PBuddyNode *node = meta->free[exp_idx];
        struct ARC_PBuddyNode *ret = NULL;

        if (node != NULL && node->canary_high == ARC_PBUDDY_CANARY_HIGH
            && node->canary_low == ARC_PBUDDY_CANARY_LOW) {
                retry1:;
                ARC_ATOMIC_XCHG(&meta->free[exp_idx], &node->next, &ret);

                if (ret == NULL) {
                        goto retry2;
                }

                if (node != ret) {
                        node = meta->free[exp_idx];
                        goto retry1;
                }

                return ret;
        }

        retry2:;

        int c = 1;
        int i = exp_idx + 1;

        for (; i < (meta->exp - PMM_BUDDY_LOWEST_EXPONENT + 1) && (node = meta->free[i]) == NULL; i++, c++);

        retry:;

        if (node == NULL) {
                ARC_DEBUG(ERR, "No base node found\n");
                return NULL;
        }

        ARC_ATOMIC_XCHG(&meta->free[i], &node->next, &ret);

        if (node != ret) {
                node = meta->free[i];
                goto retry;
        }

        if (node->canary_high != ARC_PBUDDY_CANARY_HIGH
            || node->canary_low != ARC_PBUDDY_CANARY_LOW) {
                // TODO: Should the node be placed back where it was
                //       found?
                ARC_DEBUG(ERR, "Node has improper canaries\n");
                return NULL;
        }

        for (; c > 0; c--) {
                if (pbuddy_split(meta, node) != 0) {
                        ARC_DEBUG(INFO, "Split failed, placing freeing node into %d pool (err: %d)\n", exp + c, c);
                        ARC_ATOMIC_XCHG(&meta->free[i], &node, &ret);
                        node->next = ret;

                        return NULL;
                }
        }

        return node;
}

void *pbuddy_alloc(struct ARC_PBuddy *list, size_t size) {
        SIZE_T_NEXT_POW2(size);
        int exp = __builtin_ctz(size);

        if (list == NULL || list->head == NULL
            || exp < PMM_BUDDY_LOWEST_EXPONENT || exp > list->exp) {
                ARC_DEBUG(ERR, "Improper parameters\n");
                return NULL;
        }

        spinlock_lock(&list->order_lock);
        struct ARC_PBuddyMeta *current = list->head;
        struct ARC_PBuddyMeta *last = NULL;

        while (current != NULL && current->free_objects == 0) {
                last = current;
                current = current->next;
        }

        if (current == NULL) {
                ARC_DEBUG(ERR, "Failed to find meta\n");
                spinlock_unlock(&list->order_lock);
                return NULL;
        }

        if (last != NULL) {
                last->next = list->head;
                void *tmp = list->head->next;
                list->head->next = current->next;
                current->next = tmp;
                list->head = current;
        }

        spinlock_unlock(&list->order_lock);

        return pbuddy_acquire(current, exp);
}

size_t pbuddy_free(struct ARC_PBuddy *list, void *address) {
        if (list == NULL || address == NULL) {
                return 0;
        }

        spinlock_lock(&list->order_lock);

        struct ARC_PBuddyMeta *current = list->head;

        while (current != NULL && !ADDRESS_IN_META(address, current, list->exp)) {
                current = current->next;
        }

        spinlock_unlock(&list->order_lock);

        if (current == NULL) {
                return 0;
        }
        
        return pbuddy_release(current, address);
}

int pbuddy_remove(struct ARC_PBuddy *list, struct ARC_PBuddyMeta *meta) {
        return -1;
}

int init_pbuddy(struct ARC_PBuddy *list, uintptr_t _base, int exp) {
        if (list == NULL || _base == 0
            || exp < PMM_BUDDY_LOWEST_EXPONENT || (list->exp != -1 && exp != list->exp)) {
                ARC_DEBUG(ERR, "Failed to initialize buddy allocator, improper parameters\n");
                return -1;
        }

        struct ARC_PBuddyMeta *meta = NULL;
        uint32_t ptr_count = (exp - PMM_BUDDY_LOWEST_EXPONENT + 1);
        size_t obj_size = sizeof(*meta) + ptr_count * sizeof(void *);

        size_t node_meta_size = (1 << (exp - PMM_BUDDY_LOWEST_EXPONENT)) * sizeof(struct ARC_PBuddyNodeMeta);
        struct ARC_PBuddyNodeMeta *node_metas = pmm_alloc(node_meta_size);

        if (node_metas == NULL) {
                ARC_DEBUG(ERR, "Failed to allocate per node metadata\n");
                return -2;
        }

        if (pbuddy_get_new_meta(list, &meta, obj_size) != 0) {
                ARC_DEBUG(ERR, "Failed to get new meta\n");
                pmm_free(node_metas);
                return -3;
        }

        list->exp = exp;

        memset(node_metas, 0, node_meta_size);

        // TODO: Ensure that the amount of bytes being allocated is one of the
        //       biases
        meta->node_metas = node_metas;
        meta->base = _base;
        meta->free_objects = 1;
        meta->exp = exp;

        struct ARC_PBuddyNode *node = (struct ARC_PBuddyNode *)_base;
        node->canary_high = ARC_PBUDDY_CANARY_HIGH;
        node->canary_low = ARC_PBUDDY_CANARY_LOW;
        node->next = NULL;

        meta->node_metas[0].exp = exp;
        meta->free[exp - PMM_BUDDY_LOWEST_EXPONENT] = node;

        ARC_ATOMIC_XCHG(&list->head, &meta, &meta->next);

        return 0;
}
