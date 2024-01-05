#!/bin/bash -xe

make -C uClibc-0.9.33.2
cd tester
gcc -O2 -g -Wall \
    -fPIC -mfpmath=387 \
    -I../uClibc-0.9.33.2/include \
    '-Dlibc_hidden_proto(x)=' \
    -c heap.c
cd ..

loader/build.sh tester.prg tester/heap.o


