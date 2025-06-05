/**
 * @file pmm.c
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
#include <mm/allocator.h>
#include <mm/bank.h>

struct ARC_BankNode *bank_add(struct ARC_BankMeta *meta, void *allocator) {
        if (meta == NULL) {
                return NULL;
        }

        struct ARC_BankNode *node = (struct ARC_BankNode *)meta->alloc(sizeof(*node));

        if (node == NULL) {
                return NULL;
        }

        node->allocator_meta = allocator;
        node->next = meta->first;
        meta->first = node;

        return node;
}

int bank_remove(struct ARC_BankMeta *meta, void *allocator) {
        if (meta == NULL || allocator == NULL) {
                return -1;
        }

        struct ARC_BankNode *node = meta->first;
        struct ARC_BankNode *prev = NULL;
        while (node != NULL && node->allocator_meta != allocator) {
                prev = node;
                node = node->next;
        }

        if (node == NULL) {
                return -2;
        }

        if (prev != NULL) {
                prev->next = node->next;
        } else {
                meta->first = node->next;
        }

        meta->free(node);

        return 0;
}

int init_static_bank(struct ARC_BankMeta *meta, int type, int use_alloc) {
        if (meta == NULL) {
                return -1;
        }

        switch (use_alloc) {
                case ARC_BANK_USE_ALLOC_INTERNAL: {
                        //meta->alloc = ialloc; 
                        //meta->free = ifree;
                        break;
                }
                case ARC_BANK_USE_ALLOC_GENERAL: {
                        meta->alloc = alloc; 
                        meta->free = free;
                        break;
                }
                default: {
                        return -2;
                }
        }

        meta->first = NULL;
        meta->type = type;

        return 0;
}

struct ARC_BankMeta *init_bank(int type, int use_alloc) {
        void *(*meta_alloc)(size_t size) = NULL;
	size_t (*meta_free)(void *address) = NULL;

        switch (use_alloc) {
                case ARC_BANK_USE_ALLOC_INTERNAL: {
                        //meta_alloc = ialloc; 
                        //meta_free = ifree;
                        break;
                }
                case ARC_BANK_USE_ALLOC_GENERAL: {
                        meta_alloc = alloc; 
                        meta_free = free;
                        break;
                }
                default: {
                        return NULL;
                }
        }

        struct ARC_BankMeta *meta = meta_alloc(sizeof(*meta));

        if (meta == NULL) {
                return NULL;
        }

        meta->first = NULL;
        meta->type = type;
        meta->alloc = meta_alloc;
        meta->free = meta_free;
        
        return meta;
}