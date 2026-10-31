import esphome.codegen as cg
from esphome.components import light
from esphome.components.light.effects import register_effect, validate_effects
from esphome.components.light.types import LightEffect
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_EFFECTS,
    CONF_NAME,
    CONF_OUTPUT_ID,
)

from . import CONF_JAZZLIGHTS_ID, JazzLightsComponent, jazzlights_ns

DEPENDENCIES = ["jazzlights"]

CONF_JAZZLIGHTS_CLOUDS = "jazzlights_clouds"

JazzLightsLight = jazzlights_ns.class_(
    "JazzLightsLight",
    light.LightOutput,
    cg.Component,
    cg.Parented.template(JazzLightsComponent),
)
JazzLightsCloudsEffect = jazzlights_ns.class_(
    "JazzLightsCloudsEffect", LightEffect
)


# This uses register_effect instead of register_rgb_effect so that the effect is only allowed on JazzLightsLight,
# whose effects list only allows this effect.
@register_effect(CONF_JAZZLIGHTS_CLOUDS, JazzLightsCloudsEffect, "Clouds", {})
async def jazzlights_clouds_effect_to_code(config, effect_id):
    return cg.new_Pvariable(effect_id, config[CONF_NAME])


CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(JazzLightsLight),
        cv.GenerateID(CONF_JAZZLIGHTS_ID): cv.use_id(JazzLightsComponent),
        # Transitions would fade to black before turning off hands the clouds back to their default patterns.
        cv.Optional(
            CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
        ): cv.positive_time_period_milliseconds,
        # JazzLightsLight uses the first effect to represent patterns.
        cv.Optional(
            CONF_EFFECTS, default=[{CONF_JAZZLIGHTS_CLOUDS: {}}]
        ): cv.All(
            validate_effects([CONF_JAZZLIGHTS_CLOUDS]), cv.Length(min=1, max=1)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_JAZZLIGHTS_ID])
    await light.register_light(var, config)
