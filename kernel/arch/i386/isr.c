#include <stdint.h>
#include <stdio.h>

#include <kernel/kbd.h>

#include "vector.h"

struct registers {
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha
	uint32_t int_no;   // Interrupt number
	uint32_t err_code; // Error code (or dummy 0)
	uint32_t eip, cs, eflags, useresp, ss; // Pushed by the CPU automatically
};

void isr_handler(struct registers *regs) {
	switch (regs->int_no) {
	case EXCEPTION_DOUBLE_FAULT: {
		printf("df\n");
		__asm__ __volatile__ ("cli; hlt");
		break;
	}
	case EXCEPTION_PAGE_FAULT: {
		uint32_t cr2;
		__asm__ __volatile__ ("mov %%cr2, %%eax" : "=a"(cr2) : );
		// TODO: implement %lu, %x for printf
		// printf("pf err=%lu cr2=%x\n", cr2);
		printf("pf\n");
		__asm__ __volatile__ ("cli; hlt");
		break;
	}
	case VECTOR_KEYBOARD: {
		kbd();
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
