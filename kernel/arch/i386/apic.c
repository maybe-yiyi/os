#include <cpuid.h>
#include <stdbool.h>
#include <stdint.h>

#include <kernel/apic.h>

#define CPUID_FEAT_EDX_APIC 1 << 9
#define APIC_REGISTER_ADDRESS 0x1B
#define APIC_BASE_MASK 0xFFFFF000

bool check_apic() {
	uint32_t eax, edx;
	__asm__ __volatile__ ("cpuid" : "=a" (eax), "=d" (edx) : "a" (1) : "ecx", "ebx");
	return edx & CPUID_FEAT_EDX_APIC;
}

void enable_local_apic() {
	uint32_t eax, edx;
	// find apic msr location
	__asm__ __volatile__ ("rdmsr" : "=a" (eax), "=d" (edx) : "c" (APIC_REGISTER_ADDRESS));

	// no paging implemented yet
	uint32_t base = (eax & APIC_BASE_MASK);

	// enable apic, map spurious interrupt vector register
	*(uint32_t *)(base | 0xF0) = 0x1FF;
}

void disable_pic() {
	__asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0xFF), "d"(0x21));
	__asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0xFF), "d"(0xA1));
}

void redirect_keyboard() {
	uint32_t* ioapic_base = (uint32_t*)0xFEC00000;

	ioapic_base[0] = 0x13;
	ioapic_base[0x10/4] = 0;

	ioapic_base[0] = 0x12;
	ioapic_base[0x10/4] = 0x21;
}

void enable_apic() {
	// should probably do something with this
	check_apic();

	enable_local_apic();

	// need to figure out what this actually does
	disable_pic();

	redirect_keyboard();
}
