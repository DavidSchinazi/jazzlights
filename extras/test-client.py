#!/usr/bin/python
import socket
import sys
import time
import struct

MCADDR = '239.255.223.01'
PORT = 0xDF0D

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind((MCADDR, PORT))
mreq = struct.pack("4sl", socket.inet_aton(MCADDR), socket.INADDR_ANY)
s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

while 1:
    data, addr = s.recvfrom(1024)
    (msgcode, reserved, effect, elapsed, beat, hue) = struct.unpack("!I12s16sIIB", data)
    print "RX %s:%s   %-16s elapsed: %04d beat: %04d hue: %03d" % (addr[0], addr[1], effect.rstrip('\0'), elapsed, beat, hue)
