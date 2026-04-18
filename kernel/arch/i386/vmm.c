#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>

// Current page directory
static page_directory_t* current_directory = NULL;

// Kernel page directory (aligned to 4KB)
static page_directory_t kernel_directory __attribute__((aligned(4096)));
static page_table_t kernel_table __attribute__((aligned(4096)));

// APIC page tables
static page_table_t apic_low_table __attribute__((aligned(4096)));
static page_table_t apic_high_table __attribute__((aligned(4096)));

// Get page table entry for virtual address
static uint32_t* get_page_entry(uintptr_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;
    
    uint32_t* page_table = (uint32_t*)(0xFFC00000 + (pd_idx << 12));
    return &page_table[pt_idx];
}

void vmm_init(void) {
    // Clear kernel directory and tables
    memset(kernel_directory, 0, sizeof(page_directory_t));
    memset(kernel_table, 0, sizeof(page_table_t));
    memset(apic_low_table, 0, sizeof(page_table_t));
    memset(apic_high_table, 0, sizeof(page_table_t));
    
    // Map first 4MB identity (kernel space)
    // Also map at higher half (0xC0000000)
    for (uintptr_t i = 0; i < 1024; i++) {
        uintptr_t phys = i * PAGE_SIZE;
        kernel_table[i] = phys | PAGE_PRESENT | PAGE_WRITABLE;
    }
    
    // Map kernel table to directory (identity and higher half)
    kernel_directory[0] = ((uintptr_t)kernel_table) | PAGE_PRESENT | PAGE_WRITABLE;
    kernel_directory[KERNEL_PAGE_NUMBER] = ((uintptr_t)kernel_table) | PAGE_PRESENT | PAGE_WRITABLE;
    
    // Map APIC regions
    // IO APIC at 0xFEC00000 (PD index 0x3FB, PT index 0x000)
    // Local APIC at 0xFEE00000 (PD index 0x3FB, PT index 0x800 - wait that's wrong)
    // 
    // 0xFEC00000 >> 22 = 0x3FB (1020 in decimal)
    // 0xFEE00000 >> 22 = 0x3FB as well? No...
    //
    // 0xFEC00000 = 11111110110000000000000000000000
    // >> 22 = 1111111011 = 0x3FB = 1019
    //
    // 0xFEE00000 = 11111110111000000000000000000000  
    // >> 22 = 1111111011 = 0x3FB = 1019? No wait...
    //
    // Let me recalculate:
    // 0xFEE00000 >> 22 = 0xFEE00000 / 4194304 = 0x3FB? No...
    // 0xFEE00000 = 4276092928
    // 4276092928 >> 22 = 1019 = 0x3FB
    //
    // 0xFEC00000 = 4273995776
    // 4273995776 >> 22 = 1019 = 0x3FB
    //
    // They're in the same 4MB page table! That table covers 0xFEC00000 to 0xFF000000
    
    // Actually, both APICs are in the same 4MB region (0xFEC00000-0xFF000000)
    // We need to map this 4MB region with proper page table entries
    // 0xFEC00000 >> 12 = 0xFEC00 = 1024 * 1019 + 0
    // 0xFEE00000 >> 12 = 0xFEE00 = 1024 * 1019 + 512
    
    // Set up APIC page table entries
    for (int i = 0; i < 1024; i++) {
        uintptr_t phys = 0xFEC00000 + (i * PAGE_SIZE);
        apic_low_table[i] = phys | PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE;
    }
    
    // Map APIC page table to directory entry 1019 (0x3FB)
    kernel_directory[1019] = ((uintptr_t)apic_low_table) | PAGE_PRESENT | PAGE_WRITABLE;
    
    // Map page directory to itself (for recursive mapping)
    kernel_directory[1023] = ((uintptr_t)kernel_directory) | PAGE_PRESENT | PAGE_WRITABLE;
    
    // Switch to kernel directory
    vmm_switch_directory(&kernel_directory);
    
    // Enable paging
    uint32_t cr0;
    __asm__ __volatile__ ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // PG bit
    __asm__ __volatile__ ("mov %0, %%cr0" : : "r"(cr0));
}

void vmm_map_page(uintptr_t virt, uintptr_t phys, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;
    
    // Check if page table exists
    if (!(kernel_directory[pd_idx] & PAGE_PRESENT)) {
        // Allocate new page table
        uintptr_t new_table = pmm_alloc_frame();
        if (!new_table) return;
        
        kernel_directory[pd_idx] = new_table | PAGE_PRESENT | PAGE_WRITABLE;
        
        // Clear the new page table
        uint32_t* table = (uint32_t*)(0xFFC00000 + (pd_idx << 12));
        memset(table, 0, PAGE_SIZE);
        
        vmm_flush_tlb((uintptr_t)table);
    }
    
    // Set page table entry
    uint32_t* page = get_page_entry(virt);
    *page = phys | flags;
    
    vmm_flush_tlb(virt);
}

void vmm_unmap_page(uintptr_t virt) {
    uint32_t* page = get_page_entry(virt);
    *page = 0;
    vmm_flush_tlb(virt);
}

uintptr_t vmm_get_phys(uintptr_t virt) {
    uint32_t* page = get_page_entry(virt);
    if (*page & PAGE_PRESENT) {
        return (*page & 0xFFFFF000) + (virt & 0xFFF);
    }
    return 0;
}

void* vmm_alloc_page(uint32_t flags) {
    uintptr_t phys = pmm_alloc_frame();
    if (!phys) return NULL;
    
    // Find free virtual address (simplified - just use a counter)
    static uintptr_t next_virt = 0x400000; // Start after first 4MB
    uintptr_t virt = next_virt;
    next_virt += PAGE_SIZE;
    
    vmm_map_page(virt, phys, flags | PAGE_PRESENT);
    return (void*)virt;
}

void vmm_free_page(void* virt) {
    if (!virt) return;
    
    uintptr_t phys = vmm_get_phys((uintptr_t)virt);
    if (phys) {
        pmm_free_frame(phys & 0xFFFFF000);
    }
    vmm_unmap_page((uintptr_t)virt);
}

void vmm_switch_directory(page_directory_t* dir) {
    current_directory = dir;
    __asm__ __volatile__ ("mov %0, %%cr3" : : "r"(*dir));
}

page_directory_t* vmm_get_current_directory(void) {
    return current_directory;
}