.section .text
.global gdt_flush
.type gdt_flush, @function

gdt_flush:
    # 1. Get the pointer to the GDT struct passed from C
    # In 32-bit cdecl, the first argument is at 4(%esp)
    mov 4(%esp), %eax
    lgdt (%eax)

    # 2. THE FAR JUMP
    # This loads 0x08 into the CS register and jumps to the next line.
    # Syntax: ljmp $SECTION_SELECTOR, $OFFSET
    ljmp $0x08, $.reload_segments

.reload_segments:
    # 3. Update all data segment registers to our Data Segment (0x10)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # 4. Return to the C code
    ret
