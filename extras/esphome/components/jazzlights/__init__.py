"""ESPHome external component that runs JazzLights on the clouds.

This runs the full JazzLights stack (rendering, protocol engine, NimBLE, and UDP multicast via Esp32SocketManager)
from ESPHome's loop. ESPHome manages the Wi-Fi link itself. The JazzLights sources are built as the ESP-IDF
component in extras/esphome/idf_components/jazzlights.
"""

from pathlib import Path

import esphome.codegen as cg
from esphome.components import esp32, socket
from esphome.components.esp32.const import VARIANT_ESP32, VARIANT_ESP32S3
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
    # JazzLights currently only supports the clouds on ATOM Matrix (ESP32) and ATOM S3 Lite (ESP32-S3) without the
    # Arduino core.
    esp32.only_on_variant(supported=[VARIANT_ESP32, VARIANT_ESP32S3]),
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

    is_esp32s3 = esp32.get_esp32_variant() == VARIANT_ESP32S3

    # These mirror the build flags of the clouds, clouds_s3 and vest_idf_link environments in platformio.ini.
    cg.add_build_flag("-DJL_CONFIG=CLOUDS")
    # The ATOM S3 Lite has the same clouds pins as the ATOM S3, and unlike the ATOM S3 its controller doesn't need
    # M5Unified for its screen.
    cg.add_build_flag("-DJL_CONTROLLER=" + ("ATOM_S3_LITE" if is_esp32s3 else "ATOM_MATRIX"))
    cg.add_build_flag("-DBOOT_NAME=CLOUDS-ESPHOME")
    cg.add_build_flag("-DJL_ESPHOME=1")
    cg.add_build_flag("-DJL_ESP32_NIMBLE=1")
    cg.add_build_flag("-DJL_ESP32_WIFI_LINK=1")
    if not is_esp32s3:
        # FastLED refills each LED channel's RMT memory while it sends a frame, and the LEDs flicker if a refill runs
        # late. FastLED gives each channel 2 RMT memory blocks, or 3 while Wi-Fi or Bluetooth is active. The ESP32 only
        # has 8, so we keep it at 2 so that the clouds' 4 strands can all send at the same time. The ESP32-S3 uses
        # LCD_CAM with DMA instead, see FastLedRunner::Setup().
        cg.add_build_flag("-DFASTLED_RMT_MEM_BLOCKS_NETWORK_MODE=2")
    # Our Arduino stub provides <Arduino.h>, which would otherwise make FastLED use the Arduino Serial.
    cg.add_build_flag("-DFL_ESP_USE_IDF_SERIAL=1")

    esp32.add_idf_component(name="jazzlights", path=str(_IDF_COMPONENT_PATH))
    # ESPHome excludes these by default, but FastLED needs them.
    for idf_component in (
        "app_update",
        "driver",
        "esp_adc",
        "esp_driver_i2s",
        "esp_driver_mcpwm",
        "esp_driver_rmt",
        "esp_http_server",
        "esp_lcd",
        "esp_mm",
    ):
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
    if not is_esp32s3:
        # The ESP32-S3 only supports BLE, so it doesn't have this option.
        esp32.add_idf_sdkconfig_option("CONFIG_BTDM_CTRL_MODE_BLE_ONLY", True)
    # This mirrors sdkconfig.iram.defaults.
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_IRAM_OPT", False)
    # This mirrors sdkconfig.idf.defaults.esp32, FastLED and our sources assume the board's f_cpu. Both the ESP32 and the
    # ESP32-S3 run at 240MHz.
    esp32.add_idf_sdkconfig_option("CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240", True)
    # The FastLED task and its LED interrupt run on core 1. Keep lwIP's task on core 0 like the Arduino core does, so its
    # critical sections don't delay the RMT interrupt and make the LEDs flicker.
    esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_TASK_AFFINITY_CPU0", True)
