"""Support for lights."""

import logging
from typing import Any

from homeassistant.components.light import (
    ATTR_BRIGHTNESS,
    ATTR_RGB_COLOR,
    ColorMode,
    LightEntity,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .websocket import JazzLightsWebSocketClient

LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up light."""
    host = "jazzlights-clouds.local"
    clouds = JazzLight(host)
    async_add_entities([clouds])


class JazzLight(LightEntity):
    """Defines a jazz light."""

    should_poll = False

    _attr_color_mode = ColorMode.RGB
    _attr_supported_color_modes = {ColorMode.RGB}
    _attr_has_entity_name = True

    def __init__(self, hostname: str) -> None:
        """Initialize light."""
        self._client = None
        self._hostname = hostname

    async def async_added_to_hass(self) -> None:
        """Run when this Entity has been added to HA."""
        assert self._client is None
        self._client = JazzLightsWebSocketClient(self._hostname)
        self._client.register_callback(self.async_write_ha_state)
        self._client.start()

    async def async_will_remove_from_hass(self) -> None:
        """Entity being removed from hass."""
        if self._client:
            self._client.remove_callback(self.async_write_ha_state)
            self._client.close()
            self._client = None

    @property
    def name(self) -> str:
        """Name of the entity."""
        return "Clouds"

    @property
    def brightness(self) -> int | None:
        """Return the brightness of this light between 1..255."""
        return self._client.brightness

    @property
    def rgb_color(self) -> tuple[int, int, int] | None:
        """Return the color value."""
        return self._client.rgb_color

    @property
    def is_on(self) -> bool:
        """Return the state of the light."""
        return self._client.is_on

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn off the light."""
        LOGGER.error("Turning Light Off")
        self._client.turn_off()

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn on the light."""
        brightness = kwargs.get(ATTR_BRIGHTNESS, 255)
        color = kwargs.get(ATTR_RGB_COLOR, 255)
        if not isinstance(color, tuple):
            color = None
        LOGGER.error("Turning Light On with brightness=%u color=%s", brightness, color)

        self._client.turn_on(brightness=brightness, color=color)

    @property
    def assumed_state(self) -> bool:
        """Return true if we lost connectivity with the light."""
        return self._client.assumed_state if self._client else True
