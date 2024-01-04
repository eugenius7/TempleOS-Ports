#!/bin/bash -xe

make -C uClibc-0.9.33.2
make -C headless_doom doom.a

loader/build.sh doom.prg headless_doom/doom.a


