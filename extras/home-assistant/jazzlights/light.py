"""Support for lights."""

import logging
from typing import Any

from homeassistant.components.light import (
    ATTR_BRIGHTNESS,
    ATTR_EFFECT,
    ATTR_RGB_COLOR,
    ColorMode,
    LightEntity,
    LightEntityFeature,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .websocket import JazzLightsWebSocketClient

LOGGER = logging.getLogger(__name__)

_EFFECT_CLOUDS = "clouds"


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
    _attr_supported_features = LightEntityFeature.EFFECT

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

    @property
    def effect(self) -> str | None:
        """Return the current effect of the light."""
        return _EFFECT_CLOUDS if self.rgb_color is None else None

    @property
    def effect_list(self) -> list[str]:
        """Return the list of supported effects."""
        # Note that HomeKit doesn't have any way to set or show these effects.
        return [_EFFECT_CLOUDS]

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn off the light."""
        LOGGER.error("Turning Light Off")
        self._client.turn_off()

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn on the light."""
        # Note that this function just tells us the action that the user took
        # without keeping track of any state. So if the user sets the color to
        # red, this will be called with color=(255,0,0) but then if the user
        # sets brightness to 50, this function will be called with
        # brightness=50 but no color, even though the home app UI still
        # expects the color to be red. Therefore we only send updates to
        # specific fields (e.g., only update brightness) and then we receive
        # the color in the response.
        LOGGER.error("Turning Light On kwargs=%s", kwargs)
        self._client.turn_on(
            brightness=kwargs.get(ATTR_BRIGHTNESS, None),
            color=kwargs.get(ATTR_RGB_COLOR, None),
            effect=kwargs.get(ATTR_EFFECT, None),
        )

    @property
    def assumed_state(self) -> bool:
        """Return true if we lost connectivity with the light."""
        return self._client.assumed_state if self._client else True
