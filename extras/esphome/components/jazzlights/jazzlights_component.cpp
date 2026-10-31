#include "esphome/components/jazzlights/jazzlights_component.h"

#include <esp_intr_alloc.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/log.h"
#include "jazzlights/network/esp32_socket_manager.h"
#include "jazzlights/util/instrumentation.h"
#include "jazzlights/util/log.h"

namespace esphome::jazzlights_esphome {

static const char* const TAG = "jazzlights";

namespace {

// Forwards JazzLights logs to ESPHome's logger, so they also show up over Wi-Fi. The tag is the source file name.
void LogToEsphome(char level, const char* file, int line, const char* format, va_list args) {
  int esphomeLevel;
  switch (level) {
    case 'D': esphomeLevel = ESPHOME_LOG_LEVEL_DEBUG; break;
    case 'E':
    case 'F': esphomeLevel = ESPHOME_LOG_LEVEL_ERROR; break;
    default: esphomeLevel = ESPHOME_LOG_LEVEL_INFO; break;
  }
  ::esphome::esp_log_vprintf_(esphomeLevel, file, line, format, args);
}

// Logs which interrupts are allocated on each CPU, and at which priority. This helps find what could delay the RMT
// interrupt that FastLED uses to write to the LEDs.
void LogInterrupts() {
  char* dump = nullptr;
  size_t dumpSize = 0;
  FILE* stream = open_memstream(&dump, &dumpSize);
  if (stream == nullptr) {
    ESP_LOGE(TAG, "Failed to open a stream for the interrupt dump");
    return;
  }
  const esp_err_t err = esp_intr_dump(stream);
  fclose(stream);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to dump interrupts: %s", esp_err_to_name(err));
  } else {
    // The dump is too long for a single log message, so we log it one line at a time.
    ESP_LOGCONFIG(TAG, "  Interrupts:");
    const char* line = dump;
    while (*line != '\0') {
      const char* newline = strchr(line, '\n');
      const int lineLength = newline != nullptr ? newline - line : strlen(line);
      ESP_LOGCONFIG(TAG, "    %.*s", lineLength, line);
      if (newline == nullptr) { break; }
      line = newline + 1;
    }
  }
  free(dump);
}

}  // namespace

void JazzLightsComponent::setup() {
  // This needs to happen before SetupPrimaryRunLoop() starts our other tasks.
  ::jazzlights::SetLogHandler(&LogToEsphome);
  ::jazzlights::SetupPrimaryRunLoop();
  // JazzLights renders as fast as it can, so we don't want ESPHome to sleep between loop iterations.
  this->high_frequency_loop_requester_.start();
}

void JazzLightsComponent::loop() {
  {
    JL_LOG_IF_SLOW("Adding Wi-Fi interface");
    this->maybe_add_wifi_interface_();
  }
  ::jazzlights::RunPrimaryRunLoop();
}

void JazzLightsComponent::maybe_add_wifi_interface_() {
  if (this->wifi_interface_added_) { return; }
  // ESPHome creates its Wi-Fi interface during the Wi-Fi component's setup, which runs after ours.
  esp_netif_t* netif = wifi::global_wifi_component->get_esp_netif_sta();
  if (netif == nullptr) { return; }
  uint8_t macAddress[6];
  if (esp_wifi_get_mac(WIFI_IF_STA, macAddress) != ESP_OK) { return; }
  ::jazzlights::Esp32SocketManager::Get()->AddInterface(netif, ::jazzlights::NetworkType::kWiFi,
                                                        ::jazzlights::NetworkDeviceId(macAddress));
  this->wifi_interface_added_ = true;
}

void JazzLightsComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "JazzLights:\n"
                "  Boot message: %s\n"
                "  Wi-Fi: %s",
                BOOT_MESSAGE, ::jazzlights::Esp32SocketManager::Get()->GetStatusStr().c_str());
  // TODO: remove once we know what is making the LEDs flicker.
  LogInterrupts();
}

}  // namespace esphome::jazzlights_esphome
