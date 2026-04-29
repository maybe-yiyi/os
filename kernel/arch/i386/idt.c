#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <kernel/idt.h>

struct idt_entry {
	uint16_t isr_low;
	uint16_t kernel_cs;
	uint8_t reserved;
	uint8_t attributes;
	uint16_t isr_high;
} __attribute__ ((packed));

struct idt_entry idt[256];

struct idt_pointer {
	uint16_t size;
	uint32_t offset;
} __attribute__ ((packed));

struct idt_pointer idtr;

struct registers {
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
	uint32_t int_no;   // Interrupt number
	uint32_t err_code; // Error code (or dummy 0)
	uint32_t eip, cs, eflags, useresp, ss; // Pushed by the CPU automatically
};

void isr_handler(struct registers *regs) {
	switch (regs->int_no) {
	case 8: {
		printf("timer tick fired\n");
		break;
	}
	case 13: {
		printf("gpf\n");
		__asm__ __volatile__ ("cli; hlt");
		break;
	}
	case 14: {
		printf("page fault\n");
		break;
	}
	case 33: {
		uint8_t scancode;
		__asm__ __volatile__ ("inb %%dx" : "=a" (scancode) : "d" (0x60));
		putchar(scancode);
		break;
	}
	default: {
		printf("unhandled exception\n");
		__asm__ __volatile__ ("cli; hlt");
		break;
	}
	}

	if (regs->int_no >= 32) {
		// send EOI
		uint32_t* local_apic_eoi = (uint32_t*) 0xFEE000B0;
		*local_apic_eoi = 0;
	}
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
	struct idt_entry* descriptor = &idt[vector];

	descriptor->isr_low	= (uint32_t)isr & 0xFFFF;
	descriptor->kernel_cs	= 0x08; // from GDT offset
	descriptor->attributes	= flags;
	descriptor->isr_high	= (uint32_t)isr >> 16;
	descriptor->reserved	= 0;
}

#define IDT_MAX_DESCRIPTORS 34
bool vectors[IDT_MAX_DESCRIPTORS];

extern void *isr_stub_table[];

void idt_init() {
	idtr.size = (uint16_t)sizeof(struct idt_entry) * IDT_MAX_DESCRIPTORS - 1;
	idtr.offset = (uintptr_t)&idt;

	for (uint8_t vector = 0; vector < IDT_MAX_DESCRIPTORS; vector++) {
		idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
		vectors[vector] = true;
	}

	__asm__ __volatile__ ("lidt %0" : : "m" (idtr));
	__asm__ __volatile__ ("sti");
}
