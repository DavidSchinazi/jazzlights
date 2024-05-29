#!/usr/bin/env python3

import asyncio
import pathlib
import sys

sys.path.append(
    str(
        pathlib.Path(__file__)
        .parent.parent.joinpath("home-assistant", "jazzlights")
        .resolve()
    )
)
from websocket import JazzLightsWebSocketClient

param = sys.argv[1] if len(sys.argv) > 1 else ""


async def websocket_client_main():
    client = JazzLightsWebSocketClient("jazzlights-clouds.local")

    def my_callback():
        client.remove_callback(my_callback)
        if param == "on":
            client.turn_on()
        elif param == "off":
            client.turn_off()
        else:
            client.request_status()

    client.register_callback(my_callback)
    client.start()
    await asyncio.sleep(2)


asyncio.run(websocket_client_main())
