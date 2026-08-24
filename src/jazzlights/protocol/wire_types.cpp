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

std::string displayBitsAsBinary(PatternBits p) {
  static_assert(sizeof(p) == 4, "32bits");
  char bits[33] = {};
  for (uint8_t b = 0; b < 32; b++) {
    if ((p >> b) & 1) {
      bits[b] = '.';
    } else {
      bits[b] = '_';
    }
  }
  return std::string(bits);
}

std::string networkMessageToString(const NetworkMessage& message) {
  Microseconds currentTime = timeMicros();
  char str[sizeof(", t=4294967296, p=65536, nh=255, ot=4294967296}")] = {};
  snprintf(str, sizeof(str), ", t=%u, p=%u, nh=%u, ot=%u}",
           static_cast<unsigned int>((currentTime - message.currentPatternStartTime) / kMicrosecondsPerMillisecond),
           message.precedence, message.numHops,
           static_cast<unsigned int>((currentTime - message.lastOriginationTime) / kMicrosecondsPerMillisecond));
  std::string rv = "{o=" + message.originator.toString() + ", s=" + message.sender.toString();
#if !JL_IS_CONFIG(CREATURE)
  rv += ", c=" + displayBitsAsBinary(message.currentPattern);
  rv += ", n=" + displayBitsAsBinary(message.nextPattern);
#endif  // !CREATURE
  if (message.receiptNetworkType != NetworkType::kLeading) {
    rv += ", ";
    rv += NetworkTypeToString(message.receiptNetworkType);
  }
#if JL_IS_CONFIG(CREATURE)
  char str2[sizeof(", rssi=-2147483648, rgb=010203")] = {};
  snprintf(str2, sizeof(str2), ", rssi=%d, rgb=%06x", message.receiptRssi, static_cast<int>(message.creatureColor));
  rv += str2;
#endif  // CREATURE
  if (message.orrerySceneId) {
    rv += ", os=" + std::string(OrrerySceneToString(static_cast<OrreryScene>(*message.orrerySceneId)));
  }
  rv += str;
  return rv;
}

}  // namespace jazzlights
