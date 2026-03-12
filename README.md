# OSDev

Hey! I'm following OSDev's tutorials to learn about how operating systems work.

## Getting Started
You will need to build a GCC cross compiler, which you can find instructions for [here](https://wiki.osdev.org/GCC_Cross-Compiler). (I will add internal documentation later probably.)

If you are on ARM, as one of my machines is, you may need to change the `-d` option on `grub-mkrescue` in `iso.sh` to your grub/i386-pc location is, which you may need to manually depackage the grub-pc-bin package for.
If you are on x86, you can just remove the option entirely.

To clean the source tree:
```
./clean.sh
```

To install all of the system headers into the system root without using the compiler:
```
./header.sh
```

To build the operating system:
```
./build.sh
```

...or build a bootable cdrom image of the os (without needing to run `./build.sh` separately):
```
./iso.sh
```

You can then use whatever to run the iso, such as qemu:
```
qemu-system-i386 -cdrom myos.iso
```

## Goals
I want to learn how the bootloader works, as well as memory management and scheduling.

### Disclaimer
Note: Older commits in this repository may contain bootable ISOs that include GNU GRUB, which is licensed under the GPLv3. The source code for GRUB can be found at git://git.savannah.gnu.org/grub.git.
