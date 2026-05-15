/* Declare constants for the multiboot header. */
.set ALIGN,    1<<0
.set MEMINFO,  1<<1
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC+FLAGS)

.section .multiboot.data, "aw"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bootstrap_stack, "aw", @nobits
stack_bottom:
.skip 16384
stack_top:

.section .bss, "aw", @nobits
.align 4096
boot_page_directory:
.skip 4096
boot_page_table1:
.skip 4096

.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
    # physical address of boot_page_table1
    movl $(boot_page_table1 - 0xC0000000), %edi
    # first address to map is address 0
    movl $0, %esi
    # map 1023 pages, 1024th will be VGA text buffer
    movl $1023, %ecx

1:  # only map the kernel
    cmpl $_kernel_start, %esi
    jl 2f
    cmpl $(_kernel_end - 0xC0000000), %esi
    jge 3f

    # map physical addresses as "present, writable"
    movl %esi, %edx
    orl $0x003, %edx
    movl %edx, (%edi)

2:  # size of page is 4096 bytes
    addl $4096, %esi
    # size of boot_page_table1 is 4 bytes
    addl $4, %edi
    # loop to next entry if not finished
    loop 1b

3:  # map VGA video memory to 0xC03FF000 as "present, writable"
    movl $boot_page_table1, %eax
    subl $0xC0000000, %eax
    addl $(1023 * 4), %eax
    movl $(0x000B8000 | 0x003), (%eax)

    # map page table to both virtual addresses 0x00000000 and 0xC0000000
    movl $(boot_page_table1 - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 0
    movl $(boot_page_table1 - 0xC0000000 + 0x003), boot_page_directory - 0xC0000000 + 768 * 4

    # set cr3 to the address of boot_page_directory
    movl $(boot_page_directory - 0xC0000000), %ecx
    movl %ecx, %cr3

    # enable paging and write-protect bit
    movl %cr0, %ecx
    orl $0x80010000, %ecx
    movl %ecx, %cr0

    # jump to higher half with an absolute jump
    lea 4f, %ecx
    jmp *%ecx

.section .text
4:
    # unmap identity mapping
    movl $0, boot_page_directory + 0

    # reload cr3 to force a TLB flush so the changes take effect
    movl %cr3, %ecx
    movl %ecx, %cr3

    # set up the stack
    mov $stack_top, %esp

    call kernel_main

    cli
1:  hlt
    jmp 1b
