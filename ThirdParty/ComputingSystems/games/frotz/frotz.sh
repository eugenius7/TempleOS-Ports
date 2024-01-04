#!/bin/bash -xe

make -C uClibc-0.9.33.2
make -C frotz src/frotz_common.a src/frotz_dumb.a

loader/build.sh frotz.prg frotz/src/frotz_common.a frotz/src/frotz_dumb.a

cp ~/Downloads/vgame.z8 . || true
genisoimage -o demo.iso -r -J \
    TempleLoader.CPP Frotz.CPP *.z8 *.z5 frotz.prg README*


