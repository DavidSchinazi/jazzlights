# Stands in for the Arduino core, which FastLED requires. Its include directory provides the <Arduino.h> and
# <esp32-hal.h> that FastLED includes, see extras/fastled_idf_shim/fastled_idf_shim.h.
idf_component_register(INCLUDE_DIRS "../../../fastled_idf_shim" REQUIRES esp_driver_gpio esp_rom esp_timer log soc)
target_compile_definitions(${COMPONENT_LIB} INTERFACE ESP32 F_CPU=${CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ}000000)
