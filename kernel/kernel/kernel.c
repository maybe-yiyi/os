#include <stdio.h>

#include <kernel/tty.h>

/* Check if the compiler thinks you are targeting the wrong OS. */
#if defined(__linux__)
#error "You are not using a cross-compiler, \
        certainly this is not what you want to do"
#endif

/* Check that the target is 32-bit ix86 */
#if !defined(__i386__)
#error "This needs to be compiled with a ix86-elf compiler"
#endif

size_t strlen(const char* str)
{
  size_t len = 0;
  while (str[len++]);
  return len;
}


void kernel_main(void)
{
  /* initalize terminal interface*/
  terminal_initialize();

  terminal_writestring("Hello, kernel World!\nWelcome to Yinux!\n");
}
