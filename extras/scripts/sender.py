#!/usr/bin/python3

# Test program that can send updates over IP multicast.

import socket
import struct
import sys
import time

name = ''
if len(sys.argv) >= 2:
    name = sys.argv[1].lower()

patterns = {
    'black': 0x000,
    'red': 0x100,
    'green': 0x200,
    'blue': 0x300,
    'purple': 0x400,
    'cyan': 0x500,
    'yellow': 0x600,
    'white': 0x700,
    'glowred': 0x800,
    'glowgreen': 0x900,
    'glowblue': 0xA00,
    'glowpurple': 0xB00,
    'glowcyan': 0xC00,
    'glowyellow': 0xD00,
    'glowwhite': 0xE00,
    'synctest': 0xF00,
    'calibration': 0x1000,
    'sp_rainbow': 0xE0000001,
    'sp_forest': 0xD0000001,
    'sp_party': 0xC0000001,
    'sp_cloud': 0xB0000001,
    'sp_ocean': 0xA0000001,
    'sp_lava': 0x90000001,
    'sp_heat': 0x80000001,
    'flame': 0x60000001,
    'glitter': 0x40000001,
    'threesine': 0x20000001,
    'rainbow': 0x00000001,
}
pattern = patterns.get(name, patterns['glowred'])

PORT = 0xDF0D
MCADDR = '239.255.223.01'

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
s.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 3)

# 1 byte set to 0x10
# 6 byte originator MAC address
# 6 byte sender MAC address
# 2 byte precedence
# 1 byte number of hops
# 2 bytes time delta since last origination
# 4 byte current pattern
# 4 byte next pattern
# 2 byte time delta since start of current pattern


versionByte = 0x10
thisDeviceMacAddress = b'\xFF\xFF\x01\x02\x03\x04'
precedence = 40000
numHops = 0
currentPattern = pattern
nextPattern = currentPattern
startTime = time.time()

while True:
    timeDeltaSinceOrigination = 0
    timeDeltaSinceStartOfCurrentPattern = int((time.time() - startTime) * 1000) % 10000
    messageToSend = struct.pack('!B6s6sHBHIIH',
                                versionByte, thisDeviceMacAddress, thisDeviceMacAddress,
                                precedence, numHops, timeDeltaSinceOrigination,
                                currentPattern, nextPattern, timeDeltaSinceStartOfCurrentPattern)
    # print(messageToSend)
    s.sendto(messageToSend, (MCADDR, PORT))
    time.sleep(0.1)
