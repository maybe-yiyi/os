#ifndef _KERNEL_VMM_H
#define _KERNEL_VMM_H

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096
#define PAGE_ENTRIES 1024
#define KERNEL_VIRTUAL_BASE 0xC0000000
#define KERNEL_PAGE_NUMBER (KERNEL_VIRTUAL_BASE >> 22)

// Page flags
#define PAGE_PRESENT      0x001
#define PAGE_WRITABLE     0x002
#define PAGE_USER         0x004
#define PAGE_WRITETHROUGH 0x008
#define PAGE_CACHE_DISABLE 0x010
#define PAGE_ACCESSED     0x020
#define PAGE_DIRTY        0x040
#define PAGE_SIZE_4MB     0x080  // PSE bit for 4MB pages

// Page directory and table types
typedef uint32_t page_table_t[PAGE_ENTRIES];
typedef uint32_t page_directory_t[PAGE_ENTRIES];

// Initialize virtual memory manager
void vmm_init(void);

// Map a virtual address to a physical frame
void vmm_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags);

// Unmap a virtual page
void vmm_unmap_page(uintptr_t virt);

// Get physical address for virtual address
uintptr_t vmm_get_phys(uintptr_t virt);

// Allocate a page and map it
void* vmm_alloc_page(uint32_t flags);

// Free a page
void vmm_free_page(void* virt);

// Switch page directory
void vmm_switch_directory(page_directory_t* dir);

// Get current page directory
page_directory_t* vmm_get_current_directory(void);

// Flush TLB for address
static inline void vmm_flush_tlb(uintptr_t addr) {
    __asm__ __volatile__ ("invlpg (%0)" : : "r"(addr) : "memory");
}

#endif