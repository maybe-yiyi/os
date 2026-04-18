#ifndef _KERNEL_PMM_H
#define _KERNEL_PMM_H

#include <stdint.h>
#include <stddef.h>

// Physical memory manager - manages 4KB frames
#define FRAME_SIZE 4096
#define FRAMES_PER_BYTE 8

// Mark a frame as used
void pmm_init_region(uintptr_t base, size_t size);
void pmm_deinit_region(uintptr_t base, size_t size);

// Allocate a single frame
uintptr_t pmm_alloc_frame(void);

// Free a frame
void pmm_free_frame(uintptr_t frame);

// Get total/free memory
size_t pmm_get_total_memory(void);
size_t pmm_get_free_memory(void);

#endif