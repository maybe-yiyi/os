#include <stdio.h>

#include <kernel/gdt.h>
#include <kernel/tty.h>
#include <kernel/idt.h>

void kernel_main(void)
{
	terminal_initialize();

	initialize_gdt();
	idt_init();

	/* initalize terminal interface*/
	terminal_initialize();

	terminal_writestring("Hello, kernel World!\nWelcome to Yinux!\n");
}
