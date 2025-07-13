/**
 * @file pbuddy.h
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
#ifndef ARC_MM_ALGO_PBUDDY_H
#define ARC_MM_ALGO_PBUDDY_H

#include <stdint.h>
#include <stddef.h>
#include <lib/atomics.h>
#include <mm/algo/pfreelist.h>

#define PBUDDY_CANARY_LOW 0x0
#define PBUDDY_CANARY_HIGH 0x0

struct ARC_PBuddyNode {
        uint64_t canary_low;
	struct ARC_PBuddyNode *next;
        uint64_t canary_high;
}__attribute__((packed));

struct ARC_PBuddyMeta {
	/// Next meta
	struct ARC_PBuddyMeta *next;
	/// First node.
        uintptr_t base;
	/// Number of free objects in this meta.
	size_t free_objects;

        uint32_t full_exp;
	/// Lock for everything.
	ARC_GenericSpinlock lock;
        struct ARC_PBuddyNode *nodes[];
};

struct ARC_PBuddy {
	struct ARC_PBuddyMeta *head;
        struct ARC_PFreelist meta_tables;
        ARC_GenericSpinlock order_lock;
};

void *pbuddy_alloc(struct ARC_PBuddy *list, size_t size);

size_t pbuddy_free(struct ARC_PBuddy *list, void *address);

int pbuddy_remove(struct ARC_PBuddy *list, struct ARC_PBuddyMeta *meta);

int init_pbuddy(struct ARC_PBuddy *list, uintptr_t _base, uint32_t full_exp);

#endif
