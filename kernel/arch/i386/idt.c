#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <kernel/tty.h>
#include <kernel/idt.h>
#include <kernel/keyboard.h>
#include <kernel/timer.h>
#include <kernel/scheduler.h>

// Function declarations for handlers
extern void keyboard_handler(void);
extern void timer_handler(void);

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

// Exception names
static const char* exception_names[] = {
	"Division Error",
	"Debug",
	"Non-maskable Interrupt",
	"Breakpoint",
	"Overflow",
	"Bound Range Exceeded",
	"Invalid Opcode",
	"Device Not Available",
	"Double Fault",
	"Coprocessor Segment Overrun",
	"Invalid TSS",
	"Segment Not Present",
	"Stack-Segment Fault",
	"General Protection Fault",
	"Page Fault",
	"Reserved",
	"x87 Floating-Point Exception",
	"Alignment Check",
	"Machine Check",
	"SIMD Floating-Point Exception",
	"Virtualization Exception",
	"Control Protection Exception",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved",
	"Hypervisor Injection Exception",
	"VMM Communication Exception",
	"Security Exception",
	"Reserved"
};

void isr_handler(struct registers *regs);
void isr_handler(struct registers *regs) {
	if (regs->int_no < 32) {
		// Exception
		printf("\n!!! EXCEPTION: %s (0x%x) !!!\n", 
			   exception_names[regs->int_no], regs->int_no);
		
		if (regs->int_no == 14) {
			// Page fault - get faulting address
			uint32_t cr2;
			__asm__ __volatile__ ("mov %%cr2, %0" : "=r"(cr2));
			printf("Page fault at address: 0x%x\n", cr2);
			printf("Error code: 0x%x (", regs->err_code);
			if (regs->err_code & 0x1) printf("present ");
			if (regs->err_code & 0x2) printf("write ");
			if (regs->err_code & 0x4) printf("user ");
			printf(")\n");
		}
		
		printf("EIP: 0x%x  CS: 0x%x  EFLAGS: 0x%x\n", 
			   regs->eip, regs->cs, regs->eflags);
		
		// Halt on serious exceptions
		if (regs->int_no == 8 || regs->int_no == 13 || regs->int_no == 14) {
			printf("System halted.\n");
			__asm__ __volatile__ ("cli; hlt");
		}
	} else if (regs->int_no == 32) {
		// Timer interrupt (PIT)
		timer_handler();
		scheduler_get_current(); // Mark scheduler needs reschedule
	} else if (regs->int_no == 33) {
		// Keyboard interrupt
		keyboard_handler();
	}

	if (regs->int_no >= 32) {
		// Send EOI to PIC or APIC
		if (regs->int_no >= 40) {
			// Send EOI to slave PIC
			__asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0x20), "d"(0xA0));
		}
		// Send EOI to master PIC
		__asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0x20), "d"(0x20));
		
		// Also send EOI to APIC
		uint32_t* local_apic_eoi = (uint32_t*) 0xFEE000B0;
		*local_apic_eoi = 0;
	}
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

#define IDT_MAX_DESCRIPTORS 48
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