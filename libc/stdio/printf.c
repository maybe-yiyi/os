#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool print(const char *data, size_t length)
{
	const unsigned char *bytes = (const unsigned char *)data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

char *number(char *buf, unsigned int num, unsigned int base)
{
	static const char __attribute__((nonstring)) digits[16] =
	    "0123456789abcdef";

	size_t i = sizeof(buf) - 1;
	buf[i] = '\0';

	do {
		buf[--i] = digits[num % base];
		num /= base;
	} while (num != 0);

	return buf + i;
}

int vprintf(const char *restrict format, va_list parameters)
{
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

		const char *format_begun_at = format++;

		if (*format == 'c') {
			format++;
			char c = (char)va_arg(parameters,
					      int /* char promotes to int */);
			if (!maxrem) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(&c, sizeof(c)))
				return -1;
			written++;
		} else if (*format == 'x') {
			format++;
			unsigned int num = va_arg(parameters, unsigned int);
			char buf[sizeof(num) * 2 + 1];
			char *str = number(buf, num, 16);
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
			unsigned int num = va_arg(parameters, unsigned int);
			char buf[sizeof(num) * 2 + 1];
			char *str = number(buf, num, 10);
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
		} else if (*format == 's') {
			format++;
			const char *str = va_arg(parameters, const char *);
			size_t len = strlen(str);
			if (maxrem < len) {
				// TODO: Set errno to EOVERFLOW.
				return -1;
			}
			if (!print(str, len))
				return -1;
			written += len;
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

	return written;
}

int printf(const char *restrict format, ...)
{
	va_list parameters;
	va_start(parameters, format);
	int written = vprintf(format, parameters);
	va_end(parameters);
	return written;
}
