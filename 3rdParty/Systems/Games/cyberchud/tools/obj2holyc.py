#!/bin/env python3

import argparse
import struct

parser = argparse.ArgumentParser()
parser.add_argument("file")
parser.add_argument("name")
parser.add_argument('mode', help='obj parsing mode', nargs='?', choices=('line', 'face', 'facen'), default='line')
parser.add_argument('-c', '--color', help='enable color (lines)', action='store_true')
parser.add_argument('-d', '--debug', help='print extra information', action='store_true')
args = parser.parse_args()

objFile = open(args.file)
obj = objFile.read()
objFile.close()

# Constants
BLACK    = 0
BLUE     = 1
GREEN    = 2
CYAN     = 3
RED      = 4
PURPLE   = 5
BROWN    = 6
LTGRAY   = 7
DKGRAY   = 8
LTBLUE   = 9
LTGREEN  = 10
LTCYAN   = 11
LTRED    = 12
LTPURPLE = 13
YELLOW   = 14
WHITE    = 15

COLOR_LOOKUP = {
    'BLACK': 0,
    'BLUE': 1,
    'GREEN': 2,
    'CYAN': 3,
    'RED': 4,
    'PURPLE': 5,
    'BROWN': 6,
    'LTGRAY': 7,
    'DKGRAY': 8,
    'LTBLUE': 9,
    'LTGREEN': 10,
    'LTCYAN': 11,
    'LTRED': 12,
    'LTPURPLE': 13,
    'YELLOW': 14,
    'WHITE': 15,
}

# Variables
activeColor = 'WHITE'
verts = []
lines = []
objs = []
faces = []
fns = []
normals = []
colors = []

obj = obj.splitlines()
for line in obj:
    if line[0:2] == 'v ':
        sl = line[2:].split()
        verts.append((float(sl[0]), float(sl[1]), float(sl[2])))
    elif line[0:2] == 'l ':
        sl = line[2:].split()
        lines.append((int(sl[0])-1, int(sl[1])-1))
        colors.append(activeColor)
    elif line[0:3] == 'vn ':
        sl = line[3:].split()
        normals.append((float(sl[0]), float(sl[1]), float(sl[2])))
    elif line[0:2] == 'f ':
        if args.mode == 'facen':
            sl = line[2:].split()
            fn1 = sl[0].split('/')
            fn2 = sl[1].split('/')
            fn3 = sl[2].split('/')
            faces.append((int(fn1[0])-1, int(fn2[0])-1, int(fn3[0])-1))
            fns.append((int(fn1[2])-1, int(fn2[2])-1, int(fn3[2])-1))
            colors.append(activeColor)
        else:
            sl = line[2:].split()
            faces.append((int(sl[0])-1, int(sl[1])-1, int(sl[2])-1))
    elif line[0:7] == 'usemtl ':
        activeColor = line[7:]

if args.mode == 'face':
    binOutput = bytes(len(faces).to_bytes(4, byteorder='little', signed=False))
    for tri in faces:
        binOutput += verts[tri[0]][0].to_bytes(4, byteorder='little', signed=True)
        binOutput += verts[tri[0]][1].to_bytes(4, byteorder='little', signed=True)
        binOutput += verts[tri[0]][2].to_bytes(4, byteorder='little', signed=True)
        binOutput += verts[tri[1]][0].to_bytes(4, byteorder='little', signed=True)
        binOutput += verts[tri[1]][1].to_bytes(4, byteorder='little', signed=True)
        binOutput += verts[tri[1]][2].to_bytes(4, byteorder='little', signed=True)
        binOutput += verts[tri[2]][0].to_bytes(4, byteorder='little', signed=True)
        binOutput += verts[tri[2]][1].to_bytes(4, byteorder='little', signed=True)
        binOutput += verts[tri[2]][2].to_bytes(4, byteorder='little', signed=True)
    out = open(args.name, 'wb')
    out.write(binOutput)
    out.close()
elif args.mode == 'facen':
    binOutput = bytes(len(faces).to_bytes(4, byteorder='little', signed=False))
    binOutput += len(normals).to_bytes(4, byteorder='little', signed=False)
    assert len(faces) == len(fns)
    assert len(faces) == len(colors)
    for tri in faces:
        binOutput += struct.pack('<f', verts[tri[0]][0])
        binOutput += struct.pack('<f', verts[tri[0]][1])
        binOutput += struct.pack('<f', verts[tri[0]][2])
        binOutput += struct.pack('<f', verts[tri[1]][0])
        binOutput += struct.pack('<f', verts[tri[1]][1])
        binOutput += struct.pack('<f', verts[tri[1]][2])
        binOutput += struct.pack('<f', verts[tri[2]][0])
        binOutput += struct.pack('<f', verts[tri[2]][1])
        binOutput += struct.pack('<f', verts[tri[2]][2])
    for ni in fns:
        binOutput += ni[0].to_bytes(4, byteorder='little', signed=False)
    for n in normals:
        binOutput += struct.pack('<f', n[0])
        binOutput += struct.pack('<f', n[1])
        binOutput += struct.pack('<f', n[2])
    # for color in colors:
    #     binOutput += COLOR_LOOKUP[color].to_bytes(1, byteorder='little', signed=False)
    out = open(args.name, 'wb')
    out.write(binOutput)
    out.close()
else: # lines
    lineArrB = bytes((len(lines)*2).to_bytes(4, byteorder='little', signed=False))
    for li,line in enumerate(lines):
        lineArrB += verts[lines[li][0]][0].to_bytes(4, byteorder='little', signed=True)
        lineArrB += verts[lines[li][0]][1].to_bytes(4, byteorder='little', signed=True)
        lineArrB += verts[lines[li][0]][2].to_bytes(4, byteorder='little', signed=True)
        lineArrB += verts[lines[li][1]][0].to_bytes(4, byteorder='little', signed=True)
        lineArrB += verts[lines[li][1]][1].to_bytes(4, byteorder='little', signed=True)
        lineArrB += verts[lines[li][1]][2].to_bytes(4, byteorder='little', signed=True)
    if args.color:
        for color in colors:
            lineArrB += COLOR_LOOKUP[color].to_bytes(1, byteorder='little', signed=False)
    out = open(args.name, 'wb')
    out.write(lineArrB)
    out.close()
