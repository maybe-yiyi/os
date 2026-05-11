#include <stdio.h>

#include <kernel/panic.h>

void kernel_panic(const char *restrict err)
{
	printf("%s\n", err);
	__asm__ __volatile__ ("cli ; hlt");
}

void kernel_panicf(const char *restrict format, ...)
{
	va_list parameters;
	va_start(parameters, format);
	vprintf(format, parameters);
	va_end(parameters);
	__asm__ __volatile__ ("cli ; hlt");
}
