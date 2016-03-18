#!/usr/bin/python
import socket
import sys
import time
import struct

PORT = 0xDF0D
PROTO_NAME='DFSP'
PROTO_VER_MAJOR=1
PROTO_VER_MINOR=0

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('', 0))
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

while 1:
    for color in (0xff0000ff, 0x00ff00ff):
        frame = struct.pack("!BBBBBBIII", ord(PROTO_NAME[0]), ord(PROTO_NAME[1]), 
        	ord(PROTO_NAME[2]), ord(PROTO_NAME[3]), PROTO_VER_MAJOR, PROTO_VER_MINOR, 
            1, 18, color)
        print color
        s.sendto(frame, ('<broadcast>', PORT))
        time.sleep(1)