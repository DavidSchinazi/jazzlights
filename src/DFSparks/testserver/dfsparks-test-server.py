#!/usr/bin/python
import socket
import sys
import time
import struct

PORT = 0xDF0D
MESSAGE = 0xDF0001
EFFECT_IDS = (0,1,2,3,4,5)

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('', 0))
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

while 1:
    for effect in EFFECT_IDS:
        print "effect #", effect
        frame = struct.pack("!IIII", MESSAGE, 32, effect, 0)
        s.sendto(frame, ('<broadcast>', PORT))
        time.sleep(5)