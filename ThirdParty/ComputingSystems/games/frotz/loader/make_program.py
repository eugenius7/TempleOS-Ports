#!/usr/bin/python

import struct

VERSION = 0x1

def read_qword(data, offset):
    offset *= 8
    qword = data[offset : offset + 8]
    (value, ) = struct.unpack("<Q", qword)
    return value

def make_qword(value):
    return struct.pack("<Q", value)

def make_dword(value):
    return struct.pack("<I", value)

def main():
    binary_data = open("unoffset.bin", "rb").read()
    offset_data = open("offset.bin", "rb").read()

    assert len(binary_data) == len(offset_data)
    assert binary_data != offset_data

    # examine input
    load_size = len(binary_data)
    fini_address = read_qword(binary_data, 3)
    end_address = read_qword(binary_data, 4)

    assert fini_address <= load_size, "_fini beyond EOF"
    assert end_address >= load_size, "end not beyond EOF"
    assert (load_size % 8) == 0

    # create output
    c_out = open("program", "wb+")
    c_out.write(binary_data)

    # detect locations of pointers and add relocations
    for i in xrange(fini_address / 8, load_size / 8, 1):
        diff = read_qword(offset_data, i) - read_qword(binary_data, i)
        if diff != 0:
            assert diff == 0x12340000, ("diff unexpected value", i)
            c_out.write(make_dword(i))

    # patch header
    total_size = c_out.tell()
    c_out.seek(1 * 8, 0)
    c_out.write(make_dword(VERSION))
    c_out.seek(5 * 8, 0)
    c_out.write(make_qword(load_size))
    c_out.seek(6 * 8, 0)
    c_out.write(make_qword(total_size))
    c_out.close()

if __name__ == "__main__":
    main()

