#include <stdio.h>

#include <kernel/gdt.h>
#include <kernel/tty.h>
#include <kernel/idt.h>

void kernel_main(void)
{
	/* create gdt table */
	initialize_gdt();

	/* create idt */
	idt_init();

	/* initalize terminal interface*/
	terminal_initialize();

	terminal_writestring("Hello, kernel World!\nWelcome to Yinux!\n");
}
