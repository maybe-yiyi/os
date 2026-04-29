#include <stdint.h>
#include <stdio.h>

#include <kernel/kbd.h>

void kbd() {
	uint8_t scancode;
	__asm__ __volatile__ ("inb %%dx" : "=a" (scancode) : "d" (0x60));
	putchar(scancode);
}
