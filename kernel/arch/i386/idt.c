#include <stdint.h>
#include <stdbool.h>

#include <kernel/idt.h>

extern void* isr_stub_table[];

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

void exception_handler(void);
void exception_handler() {
	__asm__ __volatile__ ("cli; hlt");
}

void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags);
void idt_set_descriptor(uint8_t vector, void* isr, uint8_t flags) {
	struct idt_entry* descriptor = &idt[vector];

	descriptor->isr_low	= (uint32_t)isr & 0xFFFF;
	descriptor->kernel_cs	= 0x08; // from GDT offset
	descriptor->attributes	= flags;
	descriptor->isr_high	= (uint32_t)isr >> 16;
	descriptor->reserved	= 0;
}

# define IDT_MAX_DESCRIPTORS 32
bool vectors[IDT_MAX_DESCRIPTORS];

extern void *isr_stub_table[];

void idt_init(void);
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
