#!/usr/bin/python3

# Test program that can send updates over IP multicast.

import socket
import struct
import sys
import time

patterns = {
    'black': 0x000,
    'red': 0x100,
    'green': 0x200,
    'blue': 0x300,
    'purple': 0x400,
    'cyan': 0x500,
    'yellow': 0x600,
    'white': 0x700,
    'glow-red': 0x800,
    'glow-green': 0x900,
    'glow-blue': 0xA00,
    'glow-purple': 0xB00,
    'glow-cyan': 0xC00,
    'glow-yellow': 0xD00,
    'glow-white': 0xE00,
    'synctest': 0xF00,
    'calibration': 0x1000,
    'follow-strand': 0x1100,
    'sp-rainbow': 0xE0000001,
    'sp-forest': 0xD0000001,
    'sp-party': 0xC0000001,
    'sp-cloud': 0xB0000001,
    'sp-ocean': 0xA0000001,
    'sp-lava': 0x90000001,
    'sp-heat': 0x80000001,
    'flame': 0x60000001,
    'glitter': 0x40000001,
    'threesine': 0x20000001,
    'rainbow': 0x00000001,
}
defaultName = 'glowred'

name = sys.argv[1] if len(sys.argv) >= 2 else ''
pattern = patterns.get(name.lower(), None)

if pattern is None:
    print('Unknown pattern "{}", using {}'.format(name, defaultName))
    pattern = patterns[defaultName]

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
