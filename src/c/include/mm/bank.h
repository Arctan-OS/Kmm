/**
 * @file pmm.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2025 awewsomegamer
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
#ifndef ARC_MM_BANK_H
#define ARC_MM_BANK_H

#include <mm/algo/pfreelist.h>
#include <mm/algo/vbuddy.h>
#include <mm/algo/pslab.h>

#define ARC_BANK_TYPE_NONE 0
#define ARC_BANK_TYPE_PFREELIST 1
#define ARC_BANK_TYPE_PSLAB 2
#define ARC_BANK_TYPE_VBUDDY 3

#define ARC_BANK_USE_ALLOC_GENERAL 0
#define ARC_BANK_USE_ALLOC_INTERNAL 1

struct ARC_BankNode {
        void *allocator_meta;
        struct ARC_BankNode *next;
};

struct ARC_BankMeta {
        struct ARC_BankNode *first;
	void *(*alloc)(size_t size);
	size_t (*free)(void *address);
        int type;
};

struct ARC_BankNode *bank_add(struct ARC_BankMeta *meta, void *allocator);
int bank_remove(struct ARC_BankMeta *meta, void *allocator);

int init_static_bank(struct ARC_BankMeta *meta, int type, int use_alloc);
struct ARC_BankMeta *init_bank(int type, int use_alloc);

#endif