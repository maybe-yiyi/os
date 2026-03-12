#include <stdio.h>

#include <kernel/tty.h>

void kernel_main(void)
{
	/* initalize terminal interface*/
	terminal_initialize();

	terminal_writestring("Hello, kernel World!\nWelcome to Yinux!\n");
}
