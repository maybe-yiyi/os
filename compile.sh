#!/bin/bash
path=$HOME/opt/cross/bin/

$path/i686-elf-as boot.s -o boot.o
$path/i686-elf-gcc -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
$path/i686-elf-gcc -T linker.ld -o myos -ffreestanding -O2 -nostdlib boot.o kernel.o -lgcc
./check.sh
cp myos isodir/boot/
grub-mkrescue -d $HOME/src/grub_i386_assets/usr/lib/grub/i386-pc/ -o myos.iso isodir
