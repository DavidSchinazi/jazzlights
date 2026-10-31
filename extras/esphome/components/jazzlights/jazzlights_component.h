#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "jazzlights/primary_runloop.h"
#include "jazzlights/render/player.h"

namespace esphome::jazzlights_esphome {

// Runs the JazzLights primary runloop from ESPHome's loop. ESPHome manages the Wi-Fi link, and this registers
// ESPHome's Wi-Fi interface with Esp32SocketManager once it exists.
class JazzLightsComponent : public Component {
 public:
  // Our Bluetooth stack needs to be initialized before esp_netif_init(), like ESPHome's esp32_ble component.
  float get_setup_priority() const override { return setup_priority::BLUETOOTH; }
  void setup() override;
  void loop() override;
  void dump_config() override;

  ::jazzlights::Player* player() const { return ::jazzlights::GetPrimaryPlayer(); }

 protected:
  void maybe_add_wifi_interface_();

  HighFrequencyLoopRequester high_frequency_loop_requester_;
  bool wifi_interface_added_ = false;
};

}  // namespace esphome::jazzlights_esphome
