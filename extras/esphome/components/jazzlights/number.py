import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_PERCENT

from . import CONF_JAZZLIGHTS_ID, JazzLightsComponent, jazzlights_ns

DEPENDENCIES = ["jazzlights"]

JazzLightsMaxBrightnessNumber = jazzlights_ns.class_(
    "JazzLightsMaxBrightnessNumber",
    number.Number,
    cg.Component,
    cg.Parented.template(JazzLightsComponent),
)

CONFIG_SCHEMA = (
    number.number_schema(
        JazzLightsMaxBrightnessNumber,
        icon="mdi:brightness-percent",
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_PERCENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_JAZZLIGHTS_ID): cv.use_id(JazzLightsComponent),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await number.new_number(config, min_value=0, max_value=100, step=1)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_JAZZLIGHTS_ID])
