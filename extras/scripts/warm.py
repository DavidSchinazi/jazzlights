#!/usr/bin/env python3

# Test program that can send updates over IP multicast.

import argparse
import contextlib
import curses
import math
import socket
import struct
import time


def convert_K_to_RGB(colour_temperature):
    """Create RGB color from temperature in Kelvin.

    Code from https://gist.github.com/petrklus/b1f427accdf7438606a6
    Algorithm from
    http://www.tannerhelland.com/4435/convert-temperature-rgb-algorithm-code/
    """
    # range check
    if colour_temperature < 1000:
        colour_temperature = 1000
    elif colour_temperature > 40000:
        colour_temperature = 40000
    tmp_internal = colour_temperature / 100.0
    # red
    if tmp_internal <= 66:
        red = 255
    else:
        tmp_red = 329.698727446 * math.pow(tmp_internal - 60, -0.1332047592)
        if tmp_red < 0:
            red = 0
        elif tmp_red > 255:
            red = 255
        else:
            red = int(tmp_red)
    # green
    if tmp_internal <= 66:
        tmp_green = 99.4708025861 * math.log(tmp_internal) - 161.1195681661
        if tmp_green < 0:
            green = 0
        elif tmp_green > 255:
            green = 255
        else:
            green = int(tmp_green)
    else:
        tmp_green = 288.1221695283 * math.pow(tmp_internal - 60, -0.0755148492)
        if tmp_green < 0:
            green = 0
        elif tmp_green > 255:
            green = 255
        else:
            green = int(tmp_green)
    # blue
    if tmp_internal >= 66:
        blue = 255
    elif tmp_internal <= 19:
        blue = 0
    else:
        tmp_blue = 138.5177312231 * math.log(tmp_internal - 10) - 305.0447927307
        if tmp_blue < 0:
            blue = 0
        elif tmp_blue > 255:
            blue = 255
        else:
            blue = int(tmp_blue)
    return red, green, blue


parser = argparse.ArgumentParser()
parser.add_argument("temperature", type=int, nargs="?", default=2700)
parser.add_argument("-a", "--admin", action="store_true")
args = parser.parse_args()
temperature = int(args.temperature)

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
precedence = 40000 if not args.admin else 60000
numHops = 0


def getTimeMillis():
    return int(time.time() * 1000)


lastSendTime = -1
startTimeThisPattern = getTimeMillis()
lastSendErrorPrintTime = -1
lastSendErrorStr = ""
MAX_ERROR_WINDOW = 5000
numPacketsSent = 0
numSendErrors = 0


def maybeSendPacket():
    global lastSendErrorStr
    global lastSendErrorPrintTime
    global lastSendTime
    global numPacketsSent
    global numSendErrors
    sendTime = getTimeMillis()
    if sendTime - lastSendTime < 100:
        return False
    lastSendTime = sendTime
    r, g, b = convert_K_to_RGB(temperature)
    currentPattern = (r << 24) | (g << 16) | (b << 8) | 0x20
    nextPattern = currentPattern
    timeDeltaSinceOrigination = 0
    timeDeltaSinceStartOfCurrentPattern = (
        getTimeMillis() - startTimeThisPattern
    ) % 10000
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
        numPacketsSent += 1
    except OSError as e:
        numSendErrors += 1
        errorTime = getTimeMillis()
        if (
            str(e) != lastSendErrorStr
            or lastSendErrorPrintTime < 0
            or errorTime - lastSendErrorPrintTime > MAX_ERROR_WINDOW
        ):
            lastSendErrorStr = str(e)
            lastSendErrorPrintTime = errorTime
            print("Send failure: {}".format(lastSendErrorStr))
    return True


def updateDisplay(stdscr):
    stdscr.addstr(4, 5, "Temperature: {: <10}".format(temperature))
    if numSendErrors > 0:
        stdscr.addstr(6, 5, "Sent: {: <10}".format(numPacketsSent))
        stdscr.addstr(7, 5, "Errors: {: <10}".format(numSendErrors))
    stdscr.refresh()


def main(stdscr):
    global temperature
    curses.curs_set(0)  # Make cursor invisible.
    curses.cbreak()  # Disable line buffering.
    stdscr.nodelay(True)  # Make getch() non-blocking.
    stdscr.addstr(0, 10, "JazzLights Color Temperature Tool")
    stdscr.addstr(1, 2, "Use arrows to change temperature.")
    stdscr.addstr(2, 2, "Press q to exit.")
    stdscr.refresh()
    key = ""
    updateDisplay(stdscr)
    while key != ord("q"):
        key = stdscr.getch()
        if key != -1:
            # stdscr.addstr(5, 0, '{: <10}'. format(key))
            # stdscr.refresh()
            if key == curses.KEY_UP:
                temperature += 100
                updateDisplay(stdscr)
            elif key == curses.KEY_DOWN:
                if temperature > 0:
                    temperature -= 100
                    updateDisplay(stdscr)
            elif key == curses.KEY_RIGHT:
                temperature += 1000
                updateDisplay(stdscr)
            elif key == curses.KEY_LEFT:
                if temperature > 1000:
                    temperature -= 1000
                else:
                    temperature = 0
                updateDisplay(stdscr)
        if maybeSendPacket():
            updateDisplay(stdscr)


with contextlib.suppress(KeyboardInterrupt):
    curses.wrapper(main)
