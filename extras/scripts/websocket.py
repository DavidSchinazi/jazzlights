#!/usr/bin/env python3

import asyncio
import socket
from sys import argv
import websockets

message = b"\x01"
if len(argv) > 1:
    cmd = argv[1].lower()
    if cmd == "on":
        message = b"\x03"
    elif cmd == "off":
        message = b"\x04"

# In theory we could just pass in the hostname to websockets, but the ESP32
# does not respond to mDNS AAAA requests and that leads websockets to hang for
# a couple seconds before trying IPv4.
host = "jazzlights-clouds.local"
address = socket.gethostbyname(host)
print("Resolved {h} to {a}".format(h=host, a=address))
uri = "ws://{a}:80/jazzlights-websocket".format(a=address)
print("Sending", message)


async def websocket_client():
    async with websockets.connect(uri) as ws:
        await ws.send(message)
        response = await ws.recv()
        print("Received", response)
        if len(response) >= 2 and response[0] == 2:
            if response[1] & 0x80 != 0:
                print("Clouds are ON")
            else:
                print("Clouds are OFF")


asyncio.run(websocket_client())
