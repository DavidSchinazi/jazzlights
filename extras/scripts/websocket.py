#!/usr/bin/env python3

import asyncio
from resolve_mdns import resolve_mdns_async
from sys import argv
import websockets

message = b"\x01"
if len(argv) > 1:
    cmd = argv[1].lower()
    if cmd == "on":
        message = b"\x03"
    elif cmd == "off":
        message = b"\x04"


async def websocket_client():
    # In theory we could just pass in the hostname to websockets, but the ESP32
    # does not respond to mDNS AAAA requests and that leads websockets to hang for
    # a couple seconds before trying IPv4. Additionally, Home Assistant Container
    # does not support mDNS due to limitations of its libc musl, so we bundle our own.
    host = "jazzlights-clouds.local"
    address = await resolve_mdns_async(host)
    print("Resolved {h} to {a}".format(h=host, a=address))
    uri = "ws://{a}:80/jazzlights-websocket".format(a=address)
    print("Sending", message)
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
