#!/usr/bin/python3

# Test program that can send updates over IP multicast.

import argparse
import random
import socket
import struct
import time

palettes = {
  'rainbow': 0x6,
  'forest': 0x5,
  'party': 0x4,
  'cloud': 0x3,
  'ocean': 0x2,
  'lava': 0x1,
  'heat': 0x0,
}

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
  'flame': 0x60000001,
  'glitter': 0x40000001,
  'threesine': 0x20000001,
  'rainbow': 0x00000001,
}
defaultName = 'glowred'

parser = argparse.ArgumentParser()
parser.add_argument('pattern')
parser.add_argument('-r', '--norandom', action='store_true')
args = parser.parse_args()

patternName = args.pattern
randomize = not args.norandom

def getPatternBytes(patternName):
  paletteName = ''
  if patternName.startswith('sp-'):
    paletteName = patternName[len('sp-'):]
    patternBytes = 0xC0000001
  elif patternName.startswith('hiphotic-'):
    patternBytes = 0xA0000001
    paletteName = patternName[len('hiphotic-'):]
  elif patternName.startswith('metaballs-'):
    patternBytes = 0x80000001
    paletteName = patternName[len('metaballs-'):]

  paletteByte = palettes.get(paletteName.lower(), None)
  if patternBytes is not None and paletteByte is not None:
    patternBytes |= paletteByte << 13

  if patternBytes is None:
    patternBytes = patterns.get(patternName.lower(), None)

  if patternBytes is None:
    print('Unknown pattern "{}"'.format(patternName))
    patternBytes = patterns[defaultName]
  if randomize and patternBytes & 0xFF != 0:
    patternBytes &= 0xFFFFFF00
    patternBytes |= random.randrange(1, 1 << 8)
    patternBytes |= random.randrange(0, 1 << 5) << 8
    patternBytes |= random.randrange(0, 1 << 13) << 16
  return patternBytes

patternBytes = getPatternBytes(patternName)

print('Using pattern {} ({:02X})'.format(patternName, patternBytes))

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
currentPattern = patternBytes
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
