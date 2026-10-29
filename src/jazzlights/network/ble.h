#ifndef JL_NETWORK_BLE_H
#define JL_NETWORK_BLE_H

#include "jazzlights/network/esp32_ble.h"
#include "jazzlights/network/esp32_ble_common.h"
#include "jazzlights/network/esp32_nimble.h"

#ifdef ESP32

namespace jazzlights {

#if JL_DISABLE_BLUETOOTH
using BleNetwork = Esp32BleNoOpNetwork;
#elif JL_ESP32_NIMBLE
using BleNetwork = Esp32NimBleNetwork;
#else   // JL_ESP32_NIMBLE
using BleNetwork = Esp32BleNetwork;
#endif  // JL_ESP32_NIMBLE

}  // namespace jazzlights

#endif  // ESP32

#endif  // JL_NETWORK_BLE_H
