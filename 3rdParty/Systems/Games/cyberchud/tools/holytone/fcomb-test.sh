#!/bin/sh

set -x
aubiopitch -s -40 -p mcomb -r 11025 -H 32 -B 48 -i "$1" >pitch.txt && ./tonepreview pitch.txt
