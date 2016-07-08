#!/usr/bin/python
import socket
import sys
import time
import struct

PORT = 0xDF0D
MESSAGE = 0xDF0002
EFFECTS = ('slowblink', 'radiaterainbow', 'rider', 'threesine', 'flame', 'glitter')

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(('', 0))
s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

while 1:
    for effect in EFFECTS:
        elapsed = beat = 0
        print "TX   %-16s elapsed: %04d beat: %04d" % (effect, elapsed, beat)
        frame = struct.pack("!I16sII", MESSAGE, effect, 0, 0)
        s.sendto(frame, ('<broadcast>', PORT))
        time.sleep(5)