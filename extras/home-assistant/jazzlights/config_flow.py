"""Config flow for JazzLights."""

from homeassistant.core import HomeAssistant
from homeassistant.helpers import config_entry_flow

from .const import DOMAIN


async def _async_has_devices(hass: HomeAssistant) -> bool:
    """Return if there are devices that can be discovered."""
    # TODO Check if there are any devices that can be discovered in the network.
    return True


config_entry_flow.register_discovery_flow(DOMAIN, "JazzLights", _async_has_devices)
