/**
 * @file pbuddy.h
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
 * Function and structure declarations for pbuddy.c.
*/
#ifndef ARC_MM_ALGO_PBUDDY_H
#define ARC_MM_ALGO_PBUDDY_H

#include "mm/algo/pfreelist.h"
#include "lib/spinlock.h"

#include <stdint.h>
#include <stddef.h>

// NOTE: Canaries can be used to signal the state of the
//       node. The bit 0 can be designated as the signal
//       bit and can be changed atomically through the use
//       of ARC_ATOMIC_INC / DEC with release semantics.
//       For instance, bit 0 on the LOW canary can be used
//       to signify an operation is being preformed on the
//       node.

#define ARC_PBUDDY_CANARY_LOW 0xAFAF1010
#define ARC_PBUDDY_CANARY_HIGH 0xCD01EF90

struct ARC_PBuddyNode {
        size_t canary_low;
        struct ARC_PBuddyNode *next;
        uint32_t canary_high;
}__attribute__((packed));

struct ARC_PBuddyNodeMeta {
        /// The exponent of the power of two for a given address.
        int exp;
        uint32_t resv0;
}__attribute__((packed));

struct ARC_PBuddyMeta {
	struct ARC_PBuddyMeta *next;
	/// First node.
        uintptr_t base;
	/// Number of free objects in this meta.
	size_t free_objects;
        /// The highest exponent of the power of two for this allocator.
	int exp;
        /// The lowest exponent of the power of two for this allocator.
        int min_exp;
        /// Metadata structures per 2^min_exp block to keep track of sizing information.
        struct ARC_PBuddyNodeMeta *node_metas; // (1 << (ARC_VBuddy.exp - PMM_BUDDY_LOWEST_EXPONENT)) nodes
        /// First free node for each power of two (ascending).
        struct ARC_PBuddyNode *free[];
};

struct ARC_PBuddy {
	struct ARC_PBuddyMeta *head;
        struct ARC_PFreelist metas;
        /// The highest exponent of the power of two for this allocator.
        int exp;
        /// The lowest exponent of the power of two for this allocator.
        int min_exp;
        /// Ordering lock for the `head` member.
        ARC_Spinlock order_lock;
};

void *pbuddy_alloc(struct ARC_PBuddy *list, size_t size);
size_t pbuddy_free(struct ARC_PBuddy *list, void *address);
int pbuddy_remove(struct ARC_PBuddy *list, struct ARC_PBuddyMeta *meta);
int init_pbuddy(struct ARC_PBuddy *list, uintptr_t _base, int exp, int min_exp);

#endif
