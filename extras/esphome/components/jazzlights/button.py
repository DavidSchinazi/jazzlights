import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv

from . import CONF_JAZZLIGHTS_ID, JazzLightsComponent, jazzlights_ns

DEPENDENCIES = ["jazzlights"]

JazzLightsNextButton = jazzlights_ns.class_(
    "JazzLightsNextButton", button.Button, cg.Parented.template(JazzLightsComponent)
)

CONFIG_SCHEMA = button.button_schema(
    JazzLightsNextButton, icon="mdi:skip-next"
).extend(
    {
        cv.GenerateID(CONF_JAZZLIGHTS_ID): cv.use_id(JazzLightsComponent),
    }
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_parented(var, config[CONF_JAZZLIGHTS_ID])
