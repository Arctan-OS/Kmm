/**
 * @file vmm.h
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
#ifndef ARC_MM_VMM_H
#define ARC_MM_VMM_H

#include <mm/algo/vbuddy.h>
#include <stddef.h>

// TODO: Is this really the best way to do this?
struct ARC_VMMMeta {
        struct ARC_VBuddyMeta buddy;
};

void *vmm_alloc(struct ARC_VMMMeta *meta, size_t size);
size_t vmm_free(struct ARC_VMMMeta *meta, void *address);
size_t vmm_len(struct ARC_VMMMeta *meta, void *address);
struct ARC_VMMMeta *init_vmm(void *base, size_t size);

#endif
