#!/bin/bash -xe

if test -z "${1}${2}"
then
    echo 'usage: build.sh <program name> <objects...>'
    exit 1
fi

cd loader
as -o setup.o setup.s 
cd ..

PROGRAM=${1}
shift
OBJS="loader/setup.o $@ uClibc-0.9.33.2/lib/libc.a"
ld -o loader/unoffset.elf -Tloader/unoffset.x $OBJS
ld -o loader/offset.elf -Tloader/offset.x $OBJS

cd loader
objcopy -O binary unoffset.elf unoffset.bin
objcopy -O binary offset.elf offset.bin

python make_program.py

gcc -o ../linuxloader.exe linuxloader.c load.c syscall.c -Wall -g
# on windows...
#gcc -m64 -o winloader.exe winloader.c load.c syscall.c winshims.s -Wall -g

mv program ../$PROGRAM

cd ..

