/**
 * @file vmm.c
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
 * The virtual memory manager can be instantiated among many processes. It uses
 * the vwatermark as its primary allocation algorithm.
*/
#include "global.h"
#include "lib/util.h"
#include "mm/algo/vwatermark.h"
#include "mm/allocator.h"
#include "mm/vmm.h"

void *vmm_alloc(struct ARC_VMMMeta *meta, size_t size) {
        return vwatermark_alloc(&meta->vwatermark, size);
}

size_t vmm_free(struct ARC_VMMMeta *meta, void *address) {
        return vwatermark_free(&meta->vwatermark, address);
}

struct ARC_VMMMeta *init_vmm(void *base, size_t size) {
        struct ARC_VMMMeta *meta = alloc(sizeof(*meta));

        if (meta == NULL) {
                return NULL;
        }

        struct ARC_VWatermarkMeta *vwatermark_meta = alloc(sizeof(*vwatermark_meta));

        if (vwatermark_meta == NULL) {
                free(vwatermark_meta);
                return NULL;
        }

        memset(meta, 0, sizeof(*meta));
        memset(vwatermark_meta, 0, sizeof(*vwatermark_meta));

        if (init_vwatermark(&meta->vwatermark, vwatermark_meta, (uintptr_t)base, size) != 0) {
                free(meta);
                free(vwatermark_meta);

                return NULL;
        }

        return meta;
}
