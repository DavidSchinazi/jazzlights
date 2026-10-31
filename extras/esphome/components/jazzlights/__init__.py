"""ESPHome external component that runs JazzLights on the clouds.

This runs the full JazzLights stack (rendering, protocol engine, NimBLE, and UDP multicast via Esp32SocketManager)
from ESPHome's loop. ESPHome manages the Wi-Fi link itself. The JazzLights sources are built as the ESP-IDF
component in extras/esphome/idf_components/jazzlights.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components import esp32, socket
from esphome.components.esp32.const import VARIANT_ESP32
import esphome.config_validation as cv
from esphome.const import CONF_ID, Framework
import esphome.final_validate as fv

CODEOWNERS = ["@DavidSchinazi"]
DEPENDENCIES = ["wifi"]
MULTI_CONF = False

CONF_JAZZLIGHTS_ID = "jazzlights_id"

# This isn't esphome::jazzlights because ESPHome's generated main.cpp has `using namespace esphome;`, which would
# make that ambiguous with the ::jazzlights namespace of the JazzLights sources.
jazzlights_ns = cg.esphome_ns.namespace("jazzlights_esphome")
JazzLightsComponent = jazzlights_ns.class_("JazzLightsComponent", cg.Component)

# Path to extras/esphome/idf_components/jazzlights. This also works when ESPHome fetches this component from git,
# since it clones the whole repository.
_IDF_COMPONENT_PATH = Path(__file__).resolve().parents[2] / "idf_components" / "jazzlights"

# These ESPHome components use Bluedroid, which conflicts with the NimBLE stack that JazzLights uses.
_BLUEDROID_COMPONENTS = (
    "esp32_ble",
    "esp32_ble_beacon",
    "esp32_ble_client",
    "esp32_ble_server",
    "esp32_ble_tracker",
    "bluetooth_proxy",
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(JazzLightsComponent),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
    cv.only_with_framework(Framework.ESP_IDF),
    # JazzLights currently only supports the clouds on ATOM Matrix without the Arduino core.
    esp32.only_on_variant(supported=[VARIANT_ESP32]),
    socket.consume_sockets(1, "jazzlights", socket.SocketType.UDP),
)


def _final_validate(config):
    full_config = fv.full_config.get()
    for component in _BLUEDROID_COMPONENTS:
        if component in full_config:
            raise cv.Invalid(
                f"jazzlights uses the NimBLE Bluetooth stack, which is incompatible with {component}"
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # These mirror the build flags of the clouds and vest_idf_link environments in platformio.ini.
    cg.add_build_flag("-DJL_CONFIG=CLOUDS")
    cg.add_build_flag("-DJL_CONTROLLER=ATOM_MATRIX")
    cg.add_build_flag("-DBOOT_NAME=CLOUDS-ESPHOME")
    cg.add_build_flag("-DJL_ESPHOME=1")
    cg.add_build_flag("-DJL_ESP32_NIMBLE=1")
    cg.add_build_flag("-DJL_ESP32_WIFI_LINK=1")

    esp32.add_idf_component(name="jazzlights", path=str(_IDF_COMPONENT_PATH))
    # ESPHome excludes these by default, but FastLED needs them.
    for idf_component in ("driver", "esp_driver_rmt", "esp_lcd"):
        esp32.include_builtin_idf_component(idf_component)

    esp32.request_bluetooth()
    esp32.request_software_coexistence()
    # These mirror sdkconfig.nimble.defaults.
    esp32.add_idf_sdkconfig_option("CONFIG_BT_BLUEDROID_ENABLED", False)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ENABLED", True)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ROLE_CENTRAL", False)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_MAX_CONNECTIONS", 1)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_SECURITY_ENABLE", False)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_NVS_PERSIST", False)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_LOG_LEVEL_NONE", True)
    esp32.add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE", 8192)
    esp32.add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_MODE_BLE_ONLY", True)
    # This mirrors sdkconfig.iram.defaults.
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_IRAM_OPT", False)
    # This mirrors sdkconfig.idf.defaults.esp32, FastLED and our sources assume the board's f_cpu.
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240", True)
