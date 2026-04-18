#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <kernel/pmm.h>

// Bitmap for frame allocation (max 4GB RAM / 4KB frames = 1M frames = 128KB bitmap)
// Place bitmap at 1MB mark in physical memory
static uint32_t* frames_bitmap = (uint32_t*)0x100000;
static uint32_t nframes = 0;
static uint32_t used_frames = 0;

#define INDEX_FROM_BIT(a) ((a) / 32)
#define OFFSET_FROM_BIT(a) ((a) % 32)

static void set_frame(uintptr_t frame_addr) {
    uint32_t frame = frame_addr / FRAME_SIZE;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    frames_bitmap[idx] |= (1 << off);
    used_frames++;
}

static void clear_frame(uintptr_t frame_addr) {
    uint32_t frame = frame_addr / FRAME_SIZE;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    if (frames_bitmap[idx] & (1 << off)) {
        frames_bitmap[idx] &= ~(1 << off);
        used_frames--;
    }
}

static bool test_frame(uintptr_t frame_addr) {
    uint32_t frame = frame_addr / FRAME_SIZE;
    uint32_t idx = INDEX_FROM_BIT(frame);
    uint32_t off = OFFSET_FROM_BIT(frame);
    return frames_bitmap[idx] & (1 << off);
}

static uintptr_t first_free_frame(void) {
    for (uint32_t i = 0; i < INDEX_FROM_BIT(nframes); i++) {
        if (frames_bitmap[i] != 0xFFFFFFFF) {
            for (uint32_t j = 0; j < 32; j++) {
                if (!(frames_bitmap[i] & (1 << j))) {
                    return (i * 32 + j) * FRAME_SIZE;
                }
            }
        }
    }
    return 0;
}

void pmm_init_region(uintptr_t base, size_t size) {
    // Align base to frame boundary
    base = (base + FRAME_SIZE - 1) & ~(FRAME_SIZE - 1);
    size = size & ~(FRAME_SIZE - 1);
    
    nframes = size / FRAME_SIZE;
    used_frames = 0;
    
    // Clear bitmap
    memset(frames_bitmap, 0, (nframes / 8) + 1);
    
    // Mark frames below 1MB and bitmap area as used
    for (uintptr_t addr = 0; addr < 0x110000; addr += FRAME_SIZE) {
        set_frame(addr);
    }
}

void pmm_deinit_region(uintptr_t base, size_t size) {
    for (uintptr_t addr = base; addr < base + size; addr += FRAME_SIZE) {
        clear_frame(addr);
    }
}

uintptr_t pmm_alloc_frame(void) {
    uintptr_t frame = first_free_frame();
    if (frame) {
        set_frame(frame);
    }
    return frame;
}

void pmm_free_frame(uintptr_t frame) {
    if (frame) {
        clear_frame(frame);
    }
}

size_t pmm_get_total_memory(void) {
    return (size_t)nframes * FRAME_SIZE;
}

size_t pmm_get_free_memory(void) {
    return (size_t)(nframes - used_frames) * FRAME_SIZE;
}