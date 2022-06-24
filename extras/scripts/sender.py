#!/usr/bin/python3

# Test program that can send updates over IP multicast.

import socket
import time
import struct

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

BLACK = 0 * 256
RED = 1
GREEN = 2 * 256
BLUE = 3 * 256
PURPLE = 4 * 256
CYAN = 5 * 256
YELLOW = 6 * 256
WHITE = 7 * 256
GLOWRED = 8 * 256
GLOWGREEN = 9 * 256
GLOWBLUE = 10 * 256
GLOWPURPLE = 11 * 256
GLOWCYAN = 12 * 256
GLOWYELLOW = 13 * 256
GLOWWHITE = 14 * 256
SYNCTEST = 15 * 256
CALIBRATION = 16 * 256

versionByte = 0x10
thisDeviceMacAddress = b'\xFF\xFF\x01\x02\x03\x04'
precedence = 3000
numHops = 0
currentPattern = PURPLE
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
