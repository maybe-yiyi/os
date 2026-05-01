#include <stdio.h>

#include <kernel/apic.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/kbd.h>
#include <kernel/tty.h>

void kernel_main(void)
{
	terminal_initialize();

	gdt_init();
	idt_init();

	enable_apic();

	kbd_init();

	printf("Hello, kernel World!\n");
	while (1) {
		if (kbd_has_key()) {
			char c = kbd_getchar();
			putchar(c);
		}
	}
}
