#ifndef JL_NETWORK_BLE_H
#define JL_NETWORK_BLE_H

#include "jazzlights/network/esp32_ble.h"
#include "jazzlights/network/esp32_ble_common.h"

#ifdef ESP32

#if JL_ESP32_NIMBLE && !JL_DISABLE_BLUETOOTH
#error "JL_ESP32_NIMBLE is not supported yet"
#endif  // JL_ESP32_NIMBLE && !JL_DISABLE_BLUETOOTH

namespace jazzlights {

#if JL_DISABLE_BLUETOOTH
using BleNetwork = Esp32BleNoOpNetwork;
#else   // JL_DISABLE_BLUETOOTH
using BleNetwork = Esp32BleNetwork;
#endif  // JL_DISABLE_BLUETOOTH

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_NETWORK_BLE_H
