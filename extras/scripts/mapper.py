#!/usr/bin/python3

# Test program that can send updates over IP multicast.

import argparse
import curses
import socket
import struct
import time

parser = argparse.ArgumentParser()
parser.add_argument('pixel_num', type=int, nargs='?', default=0)
args = parser.parse_args()
pixelNum = int(args.pixel_num)

PORT = 0xDF0D
MCADDR = '239.255.223.01'
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
thisDeviceMacAddress = b'\xFF\xFF\x01\x02\x03\x04'
precedence = 40000
numHops = 0

def getTimeMillis():
  return int(time.time() * 1000)

lastSendTime = -1
startTimeThisPattern = getTimeMillis()
lastSendErrorPrintTime = -1
lastSendErrorStr = ''
MAX_ERROR_WINDOW = 5000
numPacketsSent = 0
numSendErrors = 0

def maybeSendPacket():
  global lastSendErrorStr
  global lastSendErrorPrintTime
  global lastSendTime
  global numPacketsSent
  global numSendErrors
  global pixelNum
  sendTime = getTimeMillis()
  if sendTime - lastSendTime < 100:
     return False
  lastSendTime = sendTime
  currentPattern = 0x01000000 + (pixelNum << 8)
  nextPattern = currentPattern
  timeDeltaSinceOrigination = 0
  timeDeltaSinceStartOfCurrentPattern = (getTimeMillis() - startTimeThisPattern) % 10000
  messageToSend = struct.pack('!B6s6sHBHIIH',
                              versionByte, thisDeviceMacAddress, thisDeviceMacAddress,
                              precedence, numHops, timeDeltaSinceOrigination,
                              currentPattern, nextPattern, timeDeltaSinceStartOfCurrentPattern)
  # print(messageToSend)
  try:
    s.sendto(messageToSend, (MCADDR, PORT))
    lastSendErrorStr = ''
    numPacketsSent += 1
  except OSError as e:
    numSendErrors += 1
    errorTime = getTimeMillis()
    if str(e) != lastSendErrorStr or lastSendErrorPrintTime < 0 or errorTime - lastSendErrorPrintTime > MAX_ERROR_WINDOW:
      lastSendErrorStr = str(e)
      lastSendErrorPrintTime = errorTime
      print("Send failure: {}".format(lastSendErrorStr))
  return True


def updateDisplay(stdscr):
  stdscr.addstr(4, 5, 'Pixel: {: <10}'. format(pixelNum))
  if numSendErrors > 0:
    stdscr.addstr(6, 5, 'Sent: {: <10}'. format(numPacketsSent))
    stdscr.addstr(7, 5, 'Errors: {: <10}'. format(numSendErrors))
  stdscr.refresh()

def main(stdscr):
  global pixelNum
  curses.curs_set(0)  # Make cursor invisible.
  curses.cbreak()  # Disable line buffering.
  stdscr.nodelay(True)  # Make getch() non-blocking.
  stdscr.addstr(0, 10, 'JazzLights Mapping Tool')
  stdscr.addstr(1, 2, 'Use arrows to change pixel.')
  stdscr.addstr(2, 2, 'Press q to exit.')
  stdscr.refresh()
  key = ''
  updateDisplay(stdscr)
  while key != ord('q'):
      key = stdscr.getch()
      if key != -1:
        # stdscr.addstr(5, 0, '{: <10}'. format(key))
        # stdscr.refresh()
        if key == curses.KEY_UP:
            pixelNum += 1
            updateDisplay(stdscr)
        elif key == curses.KEY_DOWN:
            if pixelNum > 0:
              pixelNum -= 1
              updateDisplay(stdscr)
        elif key == curses.KEY_RIGHT:
            pixelNum += 10
            updateDisplay(stdscr)
        elif key == curses.KEY_LEFT:
            if pixelNum > 10:
              pixelNum -= 10
            else:
              pixelNum = 0
            updateDisplay(stdscr)
      if maybeSendPacket():
        updateDisplay(stdscr)

try:
  curses.wrapper(main)
except KeyboardInterrupt:
  pass
