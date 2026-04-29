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
		break;
	}
	case EXCEPTION_PAGE_FAULT: {
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
