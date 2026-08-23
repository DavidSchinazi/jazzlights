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

const char* OrrerySceneToString(OrreryScene scene) {
  switch (scene) {
    case OrreryScene::Paused: return "Paused";
    case OrreryScene::Realistic: return "Realistic";
    case OrreryScene::Align: return "Align";
    case OrreryScene::Silly: return "Silly";
    case OrreryScene::FocusMercury: return "FocusMercury";
    case OrreryScene::FocusVenus: return "FocusVenus";
    case OrreryScene::FocusEarth: return "FocusEarth";
    case OrreryScene::FocusMars: return "FocusMars";
    case OrreryScene::FocusJupiter: return "FocusJupiter";
    case OrreryScene::FocusSaturn: return "FocusSaturn";
    case OrreryScene::FocusUranus: return "FocusUranus";
    case OrreryScene::FocusNeptune: return "FocusNeptune";
    case OrreryScene::FocusSun: return "FocusSun";
    case OrreryScene::MercuryRetrograde: return "MercuryRetrograde";
    case OrreryScene::kInvalidScene: return "Invalid";
  }
  return "Unknown";
}

}  // namespace jazzlights
