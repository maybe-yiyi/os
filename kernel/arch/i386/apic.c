#include <cpuid.h>
#include <stdbool.h>
#include <stdint.h>

#define CPUID_FEAT_EDX_APIC 1 << 9
#define IA32_APIC_BASE 0x1B

bool check_apic() {
	uint32_t eax, edx;
	__asm__ __volatile__ ("cpuid" : "=a" (eax), "=d" (edx) : "a" (1) : "ecx", "ebx");
	return edx & CPUID_FEAT_EDX_APIC;
}

void enable_local_apic() {
	uint32_t eax, edx;
	// find apic msr location
	__asm__ __volatile__ ("rdmsr" : "=a" (eax), "=d" (edx) : "c" (IA32_APIC_BASE));

	// no paging implemented yet
	uint32_t base = eax & 0xfffff000;

	// enable apic, map spurious interrupt vector register
	__asm__ __volatile__ ("mov %0, (%1)" : : "r" (0x1FF), "r" (base | 0xF0));
}

void disable_pic() {
	__asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0xFF), "d"(0x21));
	__asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0xFF), "d"(0xA1));
}

void enable_apic() {
	check_apic();

	enable_local_apic();

	disable_pic();
}
