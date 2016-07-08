#!/usr/bin/python
import socket
import sys
import time
import struct

PORT = 0xDF0D

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('', PORT))
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

while 1:
    data, addr = s.recvfrom(1024)
    (msgcode, effect, elapsed, beat) = struct.unpack("!I16sII", data)
    print "RX %s:%s   %-16s elapsed: %04d beat: %04d" % (addr[0], addr[1], effect.rstrip('\0'), elapsed, beat)
