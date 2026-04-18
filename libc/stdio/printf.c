#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool print(const char* data, size_t length) {
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

static void print_uint(unsigned int value) {
	char buffer[32];
	int i = 0;
	if (value == 0) {
		buffer[i++] = '0';
	} else {
		while (value > 0) {
			buffer[i++] = '0' + (value % 10);
			value /= 10;
		}
	}
	// Reverse and print
	while (i > 0) {
		putchar(buffer[--i]);
	}
}

static void print_int(int value) {
	if (value < 0) {
		putchar('-');
		print_uint((unsigned int)(-value));
	} else {
		print_uint((unsigned int)value);
	}
}

static void print_hex(unsigned int value) {
	char buffer[32];
	int i = 0;
	if (value == 0) {
		buffer[i++] = '0';
	} else {
		while (value > 0) {
			int digit = value % 16;
			buffer[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
			value /= 16;
		}
	}
	// Reverse and print
	while (i > 0) {
		putchar(buffer[--i]);
	}
}

static void print_hex_ptr(uintptr_t value) {
	char buffer[32];
	int i = 0;
	if (value == 0) {
		buffer[i++] = '0';
	} else {
		while (value > 0) {
			int digit = value % 16;
			buffer[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
			value /= 16;
		}
	}
	// Print at least 8 digits for pointer
	while (i < 8) buffer[i++] = '0';
	while (i > 0) {
		putchar(buffer[--i]);
	}
}

int printf(const char* restrict format, ...) {
	va_list parameters;
	va_start(parameters, format);

	int written = 0;

	while (*format != '\0') {
		size_t maxrem = INT_MAX - written;

		if (format[0] != '%' || format[1] == '%') {
			if (format[0] == '%')
				format++;
			size_t amount = 1;
			while (format[amount] && format[amount] != '%')
				amount++;
			if (maxrem < amount) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(format, amount))
				return -1;
			format += amount;
			written += amount;
			continue;
		}

		const char* format_begun_at = format++;

		if (*format == 'c') {
			format++;
			char c = (char) va_arg(parameters, int /* char promotes to int */);
			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(&c, sizeof(c)))
				return -1;
			written++;
		} else if (*format == 's') {
			format++;
			const char* str = va_arg(parameters, const char*);
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
		} else if (*format == 'u') {
			format++;
			unsigned int value = va_arg(parameters, unsigned int);
			print_uint(value);
			written++;
		} else if (*format == 'd' || *format == 'i') {
			format++;
			int value = va_arg(parameters, int);
			print_int(value);
			written++;
		} else if (*format == 'x') {
			format++;
			unsigned int value = va_arg(parameters, unsigned int);
			print_hex(value);
			written++;
		} else if (*format == 'p') {
			format++;
			void* ptr = va_arg(parameters, void*);
			print("0x", 2);
			print_hex_ptr((uintptr_t)ptr);
			written++;
		} else {
			format = format_begun_at;
			size_t len = strlen(format);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(format, len))
				return -1;
			written += len;
			format += len;
		}
	}

	va_end(parameters);
	return written;
}