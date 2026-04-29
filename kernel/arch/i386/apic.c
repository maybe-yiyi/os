#include <cpuid.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <kernel/apic.h>

#define CPUID_FEAT_EDX_APIC 1 << 9
#define MSR_APIC_REGISTER_ADDRESS 0x1B
#define APIC_BASE_ADDRESS_MASK 0xFFFFF000

uint32_t APIC_BASE_ADDRESS;
#define SPURIOUS_IVR_OFFSET 0xF0
#define DIVIDE_CONFIG_REG_OFFSET 0x3E0
#define INITIAL_COUNT_REG_OFFSET 0x380
#define LAPIC_TIMER_MODES_OFFSET 0x320

#define IOAPIC_BASE_ADDRESS 0xFEC00000 // TODO: find this at runtime through ACPI/MADT tables

bool check_apic() {
	uint32_t eax, edx;
	__asm__ __volatile__ ("cpuid" : "=a" (eax), "=d" (edx) : "a" (1) : "ecx", "ebx");
	return edx & CPUID_FEAT_EDX_APIC;
}

void enable_local_apic() {
	uint32_t eax, edx;
	// find apic msr location
	__asm__ __volatile__ ("rdmsr" : "=a" (eax), "=d" (edx) : "c" (MSR_APIC_REGISTER_ADDRESS));

	// no paging implemented yet
	APIC_BASE_ADDRESS = (eax & APIC_BASE_ADDRESS_MASK);

	// enable apic and map spurious interrupt vector register
	*(uint32_t *)(APIC_BASE_ADDRESS | SPURIOUS_IVR_OFFSET) = 0x100 | 0xFF;
}

void disable_pic() {
	__asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0xFF), "d"(0x21));
	__asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0xFF), "d"(0xA1));
}

// redirects IRQ1 to interrupt vector 33
void redirect_keyboard() {
	// upper dword
	*(uintptr_t *)(IOAPIC_BASE_ADDRESS | 0) = 0x13;
	*(uintptr_t *)(IOAPIC_BASE_ADDRESS | 0x10) = 0;

	// lower dword
	*(uintptr_t *)(IOAPIC_BASE_ADDRESS | 0) = 0x12;
	*(uintptr_t *)(IOAPIC_BASE_ADDRESS | 0x10) = 0x21;
}

void timer_init() {
	// set divide to 16
	*(uint32_t *)(APIC_BASE_ADDRESS | DIVIDE_CONFIG_REG_OFFSET) = 0x3;

	// LVT timer, sets to interrupt vector 32 and periodic mode
	*(uint32_t *)(APIC_BASE_ADDRESS | LAPIC_TIMER_MODES_OFFSET) = 0x20 | 1 << 17;

	// set initial ticks to 10000
	*(uint32_t *)(APIC_BASE_ADDRESS | INITIAL_COUNT_REG_OFFSET) = 10000;
}

void enable_apic() {
	// should probably do something with this
	if (!check_apic()) {
		printf("no apic found!");
	}

	enable_local_apic();

	// need to figure out what this actually does
	disable_pic();

	redirect_keyboard();
	timer_init();
}
