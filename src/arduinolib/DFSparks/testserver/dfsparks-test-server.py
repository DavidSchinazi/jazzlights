#!/usr/bin/python
import socket
import sys
import time

PORT = 0xDF0D

s = socket(AF_INET, SOCK_DGRAM)
s.bind(('', 0))
s.setsockopt(SOL_SOCKET, SO_BROADCAST, 1)

while 1:
    data = repr(time.time()) + '\n'
    s.sendto(data, ('<broadcast>', MYPORT))
    time.sleep(2)