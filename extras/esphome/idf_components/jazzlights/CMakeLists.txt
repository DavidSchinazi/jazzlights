# Builds the JazzLights sources in src/jazzlights as an ESP-IDF component, for the ESPHome external component in
# extras/esphome/components/jazzlights. The JL_* configuration macros come from that component's build flags.
set(JL_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../../src")
set(JL_SHIM_HEADER "${CMAKE_CURRENT_LIST_DIR}/../../../fastled_idf_shim/fastled_idf_shim.h")
file(GLOB_RECURSE JL_SRCS "${JL_SRC_DIR}/jazzlights/*.cpp")

idf_component_register(
  SRCS ${JL_SRCS}
  INCLUDE_DIRS "${JL_SRC_DIR}"
  REQUIRES bt driver esp_driver_gpio esp_driver_rmt esp_event esp_netif esp_timer esp_wifi lwip nvs_flash
           fastled__fastled arduino-esp32)

# The Arduino core defines ESP32, which both FastLED and our sources use to detect the platform. FastLED also uses
# F_CPU, which the Arduino core gets from the board definition. These are public so that the ESPHome component sees
# them when it includes our headers.
target_compile_definitions(${COMPONENT_LIB} PUBLIC ESP32 F_CPU=${CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ}000000)

# FastLED expects the Arduino core, see extras/fastled_idf_shim/fastled_idf_shim.h. Like
# extras/scripts/fastled_idf_shim.py does for PlatformIO, we only force-include the shim into FastLED itself.
idf_component_get_property(fastled_lib fastled__fastled COMPONENT_LIB)
target_compile_options(${fastled_lib} PRIVATE -include "${JL_SHIM_HEADER}")
