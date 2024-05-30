#!/usr/bin/env python3
"""JazzLights WebSocket client."""

import asyncio
from collections.abc import Callable
import logging
import struct

import websockets

if __package__:
    from .resolve_mdns import resolve_mdns_async
else:
    from resolve_mdns import resolve_mdns_async

LOGGER = logging.getLogger(__name__)

_TYPE_STATUS_REQUEST = 1
_TYPE_STATUS_SHARE = 2
_TYPE_STATUS_SET = 3

_STATUS_FLAG_ON = 0x80


class JazzLightsWebSocketClient:
    """Websockets client class."""

    def __init__(self, hostname: str) -> None:
        """Initialize client."""
        self.hostname = hostname
        self._callbacks = set()
        self._loop = asyncio.get_event_loop()
        self._ws = None
        self._is_on = False
        self._brightness = 255
        self._should_restart = False

    def register_callback(self, callback: Callable[[], None]) -> None:
        """Register callback, called when Roller changes state."""
        self._callbacks.add(callback)

    def remove_callback(self, callback: Callable[[], None]) -> None:
        """Remove previously registered callback."""
        self._callbacks.discard(callback)

    async def _send(self, message) -> None:
        LOGGER.error("Sending %s", message)
        if self._ws is not None:
            try:
                await self._ws.send(message)
            except websockets.exceptions.WebSocketException as e:
                self._handle_failure(e)
                return

    def send(self, message) -> None:
        """Send message over WebSockets."""
        asyncio.run_coroutine_threadsafe(self._send(message), self._loop)

    def _status_set_message(self, is_on: bool, brightness: int = 255):
        brightness = max(0, min(brightness, 255))
        return struct.pack(
            "!BBB", _TYPE_STATUS_SET, _STATUS_FLAG_ON if is_on else 0, brightness
        )

    def turn_on(self, brightness: int = 255) -> None:
        """Turn on the light."""
        self.send(self._status_set_message(True, brightness))

    def turn_off(self) -> None:
        """Turn off the light."""
        self.send(self._status_set_message(False))

    def toggle(self) -> None:
        """Toggle the light."""
        if self.is_on:
            self.turn_off()
        else:
            self.turn_on()

    def request_status(self) -> None:
        """Request a status update."""
        self.send(struct.pack("!B", _TYPE_STATUS_REQUEST))

    @property
    def brightness(self) -> int | None:
        """Return the brightness of this light between 1..255."""
        return self._brightness

    @property
    def is_on(self) -> bool:
        """Return the state of the light."""
        return self._is_on

    async def _run(self) -> None:
        self._should_restart = True
        assert self._ws is None
        address = await resolve_mdns_async(self.hostname)
        LOGGER.error("Resolved %s to %s", self.hostname, address)
        assert address is not None

        uri = f"ws://{address}:80/jazzlights-websocket"
        LOGGER.error("Connecting %s", uri)
        assert self._ws is None
        try:
            ws = await websockets.connect(uri)
        except websockets.exceptions.WebSocketException as e:
            self._handle_failure(e)
            return
        assert self._ws is None
        self._ws = ws
        LOGGER.error("Connected %s", uri)
        try:
            await self._ws.send(struct.pack("!B", _TYPE_STATUS_REQUEST))
        except websockets.exceptions.WebSocketException as e:
            self._handle_failure(e)
            return
        while True:
            try:
                response = await self._ws.recv()
            except websockets.exceptions.WebSocketException as e:
                self._handle_failure(e)
                return
            LOGGER.error("Received %s", response)
            if len(response) >= 2 and response[0] == _TYPE_STATUS_SHARE:
                self._is_on = response[1] & _STATUS_FLAG_ON != 0
                self._brightness = response[2] if len(response) > 2 else 255
                LOGGER.error(
                    "Clouds are %s, brightness=%u",
                    "ON" if self._is_on else "OFF",
                    self._brightness,
                )
                callbacks = self._callbacks.copy()
                for callback in callbacks:
                    LOGGER.error("Calling a callback")
                    callback()

    def start(self) -> None:
        """Start the client."""
        asyncio.run_coroutine_threadsafe(self._run(), self._loop)

    def _close(self, disable_restart: bool) -> None:
        if disable_restart:
            self._should_restart = False
        if self._ws is not None:
            ws = self._ws
            self._ws = None
            self._loop.create_task(ws.close())

    def _handle_failure(self, e) -> None:
        LOGGER.error("Websocket failed: %s", e)
        self._close(disable_restart=False)
        if self._should_restart:
            LOGGER.error("Restarting Websocket")
            # Start it as its own stack to avoid infinite recursion.
            self._loop.create_task(self._run())

    def close(self) -> None:
        """Close the client."""
        asyncio.run_coroutine_threadsafe(self._close(disable_restart=True), self._loop)


if __name__ == "__main__":

    async def websocket_client_main():
        """Command-line tester."""
        client = JazzLightsWebSocketClient("jazzlights-clouds.local")
        client.start()
        on = True
        while True:
            on = not on
            await asyncio.sleep(2)
            LOGGER.error("Turning %s", ("on" if on else "off"))
            if on:
                client.turn_on()
            else:
                client.turn_off()

    asyncio.run(websocket_client_main())
