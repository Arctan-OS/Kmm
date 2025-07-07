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
#include <mm/algo/pbuddy.h>

void *pbuddy_alloc(struct ARC_PBuddy *list, size_t size) {
        return NULL;
}

void *pbuddy_free(struct ARC_PBuddy *list, void *address) {
        return NULL;
}

int pbuddy_add(struct ARC_PBuddy *list, struct ARC_PBuddyMeta *meta) {
        return 0;
}

struct ARC_PBuddyMeta *init_pbuddy(uintptr_t _base, uintptr_t _ceil) {
        return NULL;
}
