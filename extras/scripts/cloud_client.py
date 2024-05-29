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
    future = asyncio.get_running_loop().create_future()

    def first_callback():
        client.remove_callback(first_callback)
        client.register_callback(second_callback)
        if param == "on":
            client.turn_on()
        elif param == "off":
            client.turn_off()
        else:
            client.toggle()

    def second_callback():
        client.remove_callback(second_callback)
        future.set_result(True)

    client.register_callback(first_callback)
    client.start()
    await future
    client.close()


asyncio.run(websocket_client_main())
