#!/usr/bin/env python3

'''
mfa - minimalist file access for TempleOS

usage:
    ./mfa.py list <filename> [<local-filename>]
    ./mfa.py put <filename> [<local-filename>]
    ./mfa.py command <command>

for scripting:
    ./mfa.py <script

  - use tabs to separate arguments
  - Mfa.HC must be running in a loop

example script:

wait    5
test
put B:/AutoOSInstall.HC.Z AutoOSInstall.HC
test
command #include "B:/AutoOSInstall"
test
command OSInstall(FALSE);
test

'''

import socket
import sys
import time

TCP_IP = '127.0.0.1'
TCP_PORT = 7770

CHUNK_SIZE = 8
BAUD = 115200

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
connecting = True
while connecting:
    try:
        sock.connect('/tmp/temple')
        connecting = False
    except ConnectionRefusedError:
        print('Retrying connection...')
    except FileNotFoundError:
        print('Retrying connection...')

def read_line():
    s = b''
    while True:
        ch = sock.recv(1)
        if ch == b'\n':
            print('<', s.decode())
            return s.decode()
        else:
            s += ch

def read_line_nonblock():
    s = b''
    sock.setblocking(False)
    try:
        ch = sock.recv(1)
        s += ch
    except BlockingIOError:
        sock.setblocking(True)
        return s, False

    sock.setblocking(True)
    while True:
        ch = sock.recv(1)
        if ch == b'\n':
            print('<', s.decode())
            return s.decode(), True
        else:
            s += ch

def read_bytes(count):
    s = b''
    while len(s) < count:
        s += sock.recv(1)
    return s

def send(line):
    print('>', line)
    sock.send((line + "\n").encode())

def test():
    send('?')
    assert read_line() == '!'

def do_command(*argv):
    cmd = argv[0]

    if cmd == 'put':
        filename = argv[1]
        local_filename = argv[2] if len(argv) > 2 else filename

        with open(local_filename, 'rb') as f:
            data = f.read()
            dataTX = data
            size = len(data)

            test()
            send('P' + filename)
            send('S' + str(size))

            while len(dataTX):
                sock.send(dataTX[0:CHUNK_SIZE])
                dataTX = dataTX[CHUNK_SIZE:]
                val, status = read_line_nonblock()
                if (status == True):
                    val = int(val)
                    print(val, status, val&0xff)
                    time.sleep(0.4)
                    sock.send((0).to_bytes(1,'little'))
                    sock.send((val&0xff).to_bytes(1, 'little'))
                    print('Resuming...', len(dataTX), sock.recv(1));
                    dataTX = data[size-val:]
                    print('New: ', len(dataTX))
                print('\r', len(dataTX), end='')
                time.sleep(float(CHUNK_SIZE) * 8 / BAUD)

        print('Read', size, 'bytes from', local_filename)
    elif cmd == 'list':
        filename = argv[1]
        local_filename = argv[2] if len(argv) > 2 else filename

        send('L' + filename)

        next_ = read_line()
        assert next_[0] == 'S'
        size = int(next_[1:])
        size_remaining = size

        with open(local_filename, 'wb') as f:
            while size_remaining:
                read_now = min(size_remaining, 0x1000)
                data = read_bytes(read_now)
                f.write(data)
                size_remaining -= len(data)

        print('Written', size, 'bytes to', local_filename)
    elif cmd == 'command':
        command = argv[1]

        test()
        send("'" + command)
    elif cmd == 'wait':
        secs = float(argv[1])
        time.sleep(secs)
    else:
        raise Exception('Command error: ' + argv[0])

if len(sys.argv) > 1:
    do_command(*sys.argv[1:])
else:
    try:
        while True:
            if sys.version_info[0] < 3:
                entry = raw_input()
            else:
                entry = input()

            if len(entry) and entry[0] != '#':
                items = entry.split('\t')
                do_command(*items)
    except(EOFError):
        pass
