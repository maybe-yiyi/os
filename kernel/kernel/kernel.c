#include <stdio.h>

#include <kernel/gdt.h>
#include <kernel/tty.h>

void kernel_main(void)
{
	/* create gdt table */
	initialize_gdt();

	/* initalize terminal interface*/
	terminal_initialize();

	terminal_writestring("Hello, kernel World!\nWelcome to Yinux!\n");
}
