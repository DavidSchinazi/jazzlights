#!/usr/bin/env python3

# Test program that can send updates over IP multicast.

import argparse
import random
import socket
import struct
from sys import exit
import time

palettes = {
    "rainbow": 0x6,
    "forest": 0x5,
    "party": 0x4,
    "cloud": 0x3,
    "ocean": 0x2,
    "lava": 0x1,
    "heat": 0x0,
}

patterns = {
    "black": 0x0,
    "red": 0x100,
    "green": 0x200,
    "blue": 0x300,
    "purple": 0x400,
    "cyan": 0x500,
    "yellow": 0x600,
    "white": 0x700,
    "glow-red": 0x800,
    "glow-green": 0x900,
    "glow-blue": 0xA00,
    "glow-purple": 0xB00,
    "glow-cyan": 0xC00,
    "glow-yellow": 0xD00,
    "glow-white": 0xE00,
    "synctest": 0xF00,
    "calibration": 0x1000,
    "follow-strand": 0x1100,
    "glitter": 0x1200,
    "the-matrix": 0x1300,
    "threesine": 0x1400,
    "sp": 0xC0000001,
    "hiphotic": 0x80000001,
    "flame": 0x40000001,
    "rings": 0x00000001,
}
defaultName = "glow-red"

parser = argparse.ArgumentParser()
parser.add_argument("pattern")
parser.add_argument("-r", "--norandom", action="store_true")
args = parser.parse_args()

patternName = args.pattern
randomize = not args.norandom


def randomizePattern(patternBytes, randomize=True):
    if randomize and ((patternBytes & 0xF) != 0 or (patternBytes & 0xFF) == 0x30):
        reservedWithPalette = (patternBytes & 0xFF) == 0x30
        patternBytes &= 0xC000E000  # 13-15 for palette and 30-31 for pattern
        if reservedWithPalette:
            patternBytes |= 0x030
        else:
            patternBytes |= random.randrange(
                1, 1 << 4
            )  # bits 0-3, but must not be all-zero
            patternBytes |= random.randrange(0, 1 << 9) << 4  # bits 4-12
        patternBytes |= random.randrange(0, 1 << 14) << 16  # bits 16-29
    return patternBytes


def getPatternBytes(patternName):
    if patternName.startswith("mapping-"):
        pixelNum = int(patternName[len("mapping-") :])
        return 0x00000010 | (pixelNum << 8)
    if patternName.startswith("coloring-"):
        rgb = int(patternName[len("coloring-") :], 16)
        return 0x00000020 | (rgb << 8)
    patternBytes = None
    paletteName = ""
    if patternName.startswith("sp-"):
        paletteName = patternName[len("sp-") :]
        patternBytes = 0xC0000001
    elif patternName.startswith("hiphotic-"):
        patternBytes = 0x80000001
        paletteName = patternName[len("hiphotic-") :]
    elif patternName.startswith("metaballs-"):
        patternBytes = 0x80000030
        paletteName = patternName[len("metaballs-") :]
    elif patternName.startswith("bursts-"):
        patternBytes = 0x00000030
        paletteName = patternName[len("bursts-") :]
    elif patternName.startswith("rings-"):
        patternBytes = 0x00000001
        paletteName = patternName[len("rings-") :]
    elif patternName.startswith("flame-"):
        patternBytes = 0x40000001
        paletteName = patternName[len("flame-") :]

    paletteByte = palettes.get(paletteName.lower(), None)
    if patternBytes is not None and paletteByte is not None:
        patternBytes |= paletteByte << 13

    if patternBytes is None:
        patternBytes = patterns.get(patternName.lower(), None)

    if patternBytes is None:
        print('Unknown pattern "{}"'.format(patternName))
        patternBytes = patterns[defaultName]
    return randomizePattern(patternBytes, randomize)


patternBytes = getPatternBytes(patternName)

print("Using pattern {} ({:08X})".format(patternName, patternBytes))

PORT = 6699
MCADDR = "224.0.0.169"
PATTERN_DURATION = 10000

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
thisDeviceMacAddress = b"\xff\xff\x01\x02\x03\x04"
precedence = 40000
numHops = 0
currentPattern = patternBytes
nextPattern = randomizePattern(currentPattern, randomize)


def getTimeMillis():
    return int(time.time() * 1000)


startTimeThisPattern = getTimeMillis()
lastSendErrorPrintTime = -1
lastSendErrorStr = ""
MAX_ERROR_WINDOW = 5000

while True:
    timeDeltaSinceOrigination = 0
    while getTimeMillis() - startTimeThisPattern >= PATTERN_DURATION:
        startTimeThisPattern += PATTERN_DURATION
        currentPattern = nextPattern
        print("Using pattern {} ({:08X})".format(patternName, currentPattern))
        nextPattern = randomizePattern(nextPattern, randomize)
    timeDeltaSinceStartOfCurrentPattern = getTimeMillis() - startTimeThisPattern
    messageToSend = struct.pack(
        "!B6s6sHBHIIH",
        versionByte,
        thisDeviceMacAddress,
        thisDeviceMacAddress,
        precedence,
        numHops,
        timeDeltaSinceOrigination,
        currentPattern,
        nextPattern,
        timeDeltaSinceStartOfCurrentPattern,
    )
    # print(messageToSend)
    try:
        s.sendto(messageToSend, (MCADDR, PORT))
        lastSendErrorStr = ""
    except OSError as e:
        errorTime = getTimeMillis()
        if (
            str(e) != lastSendErrorStr
            or lastSendErrorPrintTime < 0
            or errorTime - lastSendErrorPrintTime > MAX_ERROR_WINDOW
        ):
            lastSendErrorStr = str(e)
            lastSendErrorPrintTime = errorTime
            print("Send failure: {}".format(lastSendErrorStr))
    try:
        time.sleep(0.1)
    except KeyboardInterrupt:
        print()
        exit(0)
