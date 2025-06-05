/**
 * @file vmm.c
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
#include <mm/vmm.h>
#include <mm/allocator.h>
#include <global.h>
#include <lib/util.h>

void *vmm_alloc(struct ARC_VMMMeta *meta, size_t size) {
        if (size == 0 || meta == NULL) {
                return NULL;
        }

        return vbuddy_alloc(&meta->buddy, size);
}

size_t vmm_free(struct ARC_VMMMeta *meta, void *address) {
        if (meta == NULL) {
                return 0;
        }
        
        return vbuddy_free(&meta->buddy, address);
}

size_t vmm_len(struct ARC_VMMMeta *meta, void *address) {
        if (meta == NULL) {
                return 0;
        }

        return vbuddy_len(&meta->buddy, address);
}

struct ARC_VMMMeta *init_vmm(void *base, size_t size) {
        struct ARC_VMMMeta *meta = (struct ARC_VMMMeta *)alloc(sizeof(*meta));

        if (meta == NULL) {
                ARC_DEBUG(ERR, "Failed to allocate VMM meta\n");
                return NULL;
        }

        memset(meta, 0, sizeof(*meta));

        meta->buddy.ialloc = alloc;
        meta->buddy.ifree = free;

        if (init_vbuddy(&meta->buddy, base, size, PAGE_SIZE) != 0) {
                ARC_DEBUG(ERR, "Failed to initialize vbuddy allocator\n");
                free(meta);
                return NULL;
        }

        return meta;
}
