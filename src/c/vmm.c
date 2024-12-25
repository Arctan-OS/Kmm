/**
 * @file vmm.c
 *
 * @author awewsomegamer <awewsomegamer@gmail.com>
 *
 * @LICENSE
 * Arctan - Operating System Kernel
 * Copyright (C) 2023-2024 awewsomegamer
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
#include <mm/vmm.h>
#include <mm/algo/buddy.h>
#include <arch/pager.h>
#include <lib/util.h>

static struct ARC_BuddyMeta vmm_meta = { 0 };

void *vmm_alloc(size_t size) {
	void *virtual = buddy_alloc(&vmm_meta, size);

	if (virtual == NULL) {
		ARC_DEBUG(ERR, "Failed to allocate\n");
		return NULL;
	}

	if (pager_fly_map((uintptr_t)virtual, size, 0) != 0) {
		ARC_DEBUG(ERR, "Failed to fly map %p (%lu B)\n", virtual, size);
		buddy_free(&vmm_meta, virtual);
		return NULL;
	}

	return virtual;
}

void *vmm_free(void *address) {
	size_t freed = buddy_free(&vmm_meta, address);

	if (freed == 0) {
		return NULL;
	}

	if (pager_fly_unmap((uintptr_t)address, freed) != 0) {
		return NULL;
	}

	return address;
}

void *vmm_alloc_nopage(size_t size) {
	if (size < PAGE_SIZE) {
		size = PAGE_SIZE;
	}

	return buddy_alloc(&vmm_meta, size);
}

size_t vmm_free_nopage(void *address) {
	return buddy_free(&vmm_meta, address);
}

int init_vmm(void *addr, size_t size) {
	return init_buddy(&vmm_meta, addr, size, PAGE_SIZE);
}
