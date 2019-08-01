#!/usr/bin/python
import socket
import sys
import time
import struct

PORT = 0xDF0D
MCADDR = '239.255.223.01'
MESSAGE = 0xDF0002
EFFECTS = ('slowblink', 'rainbow', 'rider', 'threesine', 'flame', 'glitter')

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 3)

while 1:
    for effect in EFFECTS:
        elapsed = beat = hue_med = hue_dev = 0
        print "TX   %-16s elapsed: %04d beat: %04d hue_med:%03d hue_dev:%03d" % (effect, elapsed, beat, hue_med, hue_dev)
        #frame = struct.pack("!I16sII", MESSAGE, effect, 0, 0) # bad!
        frame = struct.pack("!I12s16sIIBB", MESSAGE, '', effect, elapsed, beat, hue_med, hue_dev)
        s.sendto(frame, (MCADDR, PORT))
        time.sleep(5)
