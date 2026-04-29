#include <stdio.h>

#include <kernel/gdt.h>
#include <kernel/tty.h>
#include <kernel/idt.h>
#include <kernel/apic.h>

void kernel_main(void)
{
	terminal_initialize();

	gdt_init();
	idt_init();

	enable_apic();

	printf("Hello, kernel World!\n");
}
