"""Support for lights."""

from typing import Any

import websockets

from homeassistant.components.light import ColorMode, LightEntity, LightEntityFeature
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import LOGGER
from .resolve_mdns import resolve_mdns_async


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up light."""
    host = "jazzlights-clouds.local"
    address = await resolve_mdns_async(host)
    LOGGER.error("Resolved %s to %s", host, address)
    if address is None:
        address = "10.41.40.191"
    if address is not None:
        uri = f"ws://{address}:80/jazzlights-websocket"
        LOGGER.error("Connecting %s", uri)
        ws = await websockets.connect(uri)
        clouds = JazzLight(ws)
        LOGGER.error("Q async_setup_entry")
        async_add_entities([clouds])


class JazzLight(LightEntity):
    """Defines a jazz light."""

    _attr_color_mode = ColorMode.BRIGHTNESS
    _attr_translation_key = "main"
    _attr_supported_features = LightEntityFeature.TRANSITION
    _attr_supported_color_modes = {ColorMode.BRIGHTNESS}
    _attr_has_entity_name = True

    def __init__(self, ws) -> None:
        """Initialize light."""
        self._my_state = True
        self._ws = ws
        LOGGER.error("Init JazzLight")

    @property
    def name(self) -> str:
        """Name of the entity."""
        return "Clouds"

    @property
    def brightness(self) -> int | None:
        """Return the brightness of this light between 1..255."""
        return 255

    @property
    def is_on(self) -> bool:
        """Return the state of the light."""
        return self._my_state

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn off the light."""
        LOGGER.error("Turning Light Off")
        self._my_state = False
        await self._ws.send(b"\x04")

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn on the light."""
        LOGGER.error("Turning Light On")
        self._my_state = True
        await self._ws.send(b"\x03")
