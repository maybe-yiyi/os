#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdarg.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int vprintf(const char *__restrict, va_list);
int printf(const char *__restrict, ...) __attribute__ ((format(printf, 1, 2)));
int putchar(int);
int puts(const char *);

#ifdef __cplusplus
}
#endif

#endif
