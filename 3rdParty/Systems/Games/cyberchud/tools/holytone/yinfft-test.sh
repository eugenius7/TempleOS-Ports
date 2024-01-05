#!/bin/sh

set -x
aubiopitch -s -40 -l 0.90 -p yinfft -r 11025 -H 128 -B 256 -i "$1" >pitch.txt && ./tonepreview pitch.txt
