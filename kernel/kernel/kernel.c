#include <stdio.h>

#include <kernel/gdt.h>
#include <kernel/tty.h>
#include <kernel/idt.h>

void kernel_main(void)
{
	terminal_initialize();

	gdt_init();
	idt_init();

	terminal_writestring("Hello, kernel World!\n");
}
