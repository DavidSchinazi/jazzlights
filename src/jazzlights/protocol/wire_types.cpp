#include "jazzlights/protocol/wire_types.h"

namespace jazzlights {

const char* NetworkTypeToString(NetworkType type) {
  switch (type) {
    case NetworkType::kLeading: return "Lead";
    case NetworkType::kBLE: return "BLE";
    case NetworkType::kWiFi: return "Wi-Fi";
    case NetworkType::kEthernet: return "Eth";
    case NetworkType::kOther: return "Other";
  }
  return "???";
}

NetworkDeviceId NetworkDeviceId::PlusOne() const {
  NetworkDeviceId deviceId = *this;
  deviceId.data()[5]++;
  if (deviceId.data()[5] == 0) {
    deviceId.data()[4]++;
    if (deviceId.data()[4] == 0) {
      deviceId.data()[3]++;
      if (deviceId.data()[3] == 0) {
        deviceId.data()[2]++;
        if (deviceId.data()[2] == 0) {
          deviceId.data()[1]++;
          if (deviceId.data()[1] == 0) { deviceId.data()[0]++; }
        }
      }
    }
  }
  return deviceId;
}

}  // namespace jazzlights
