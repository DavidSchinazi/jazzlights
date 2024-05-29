#!/usr/bin/env python3
"""JazzLights WebSocket client."""

import asyncio
import logging
from collections.abc import Callable

import websockets

from .resolve_mdns import resolve_mdns_async

_LOGGER = logging.getLogger(__name__)


class JazzLightsWebSocketClient:
    """Websockets client class."""

    def __init__(self, hostname: str) -> None:
        """Initialize client."""
        self.hostname = hostname
        self._callbacks = set()
        self._loop = asyncio.get_event_loop()
        self._ws = None
        self._is_on = False

    def register_callback(self, callback: Callable[[], None]) -> None:
        """Register callback, called when Roller changes state."""
        self._callbacks.add(callback)

    def remove_callback(self, callback: Callable[[], None]) -> None:
        """Remove previously registered callback."""
        self._callbacks.discard(callback)

    async def _send(self, message) -> None:
        _LOGGER.error("Sending %s", message)
        if self._ws is not None:
            await self._ws.send(message)

    def send(self, message) -> None:
        """Send message over WebSockets."""
        asyncio.run_coroutine_threadsafe(self._send(message), self._loop)

    def turn_on(self) -> None:
        """Turn on the light."""
        self.send(b"\x03")

    def turn_off(self) -> None:
        """Turn off the light."""
        self.send(b"\x04")

    def request_status(self) -> None:
        """Request a status update."""
        self.send(b"\x01")

    @property
    def is_on(self) -> bool:
        """Return the state of the light."""
        return self._is_on

    async def _run(self) -> None:
        while True:
            address = await resolve_mdns_async(self.hostname)
            _LOGGER.error("Resolved %s to %s", self.hostname, address)
            if address is None:
                continue
            uri = f"ws://{address}:80/jazzlights-websocket"
            _LOGGER.error("Connecting %s", uri)
            self._ws = await websockets.connect(uri)
            _LOGGER.error("Connected %s", uri)
            await self._ws.send(b"\x01")
            while True:
                response = await self._ws.recv()
                _LOGGER.error("Received %s", response)
                if len(response) >= 2 and response[0] == 2:
                    self._is_on = response[1] & 0x80 != 0
                    if self._is_on:
                        _LOGGER.error("Clouds are ON")
                    else:
                        _LOGGER.error("Clouds are OFF")
                    for callback in self._callbacks:
                        _LOGGER.error("Calling a callback")
                        callback()

    def start(self) -> None:
        """Start the client."""
        self._loop.create_task(self._run())


if __name__ == "__main__":

    async def websocket_client_main():
        """Command-line tester."""
        client = JazzLightsWebSocketClient("jazzlights-clouds.local")
        client.start()
        on = True
        while True:
            on = not on
            await asyncio.sleep(2)
            _LOGGER.error("Turning %s", ("on" if on else "off"))
            if on:
                client.turn_on()
            else:
                client.turn_off()

    asyncio.run(websocket_client_main())
