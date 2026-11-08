// WLED usermod that runs the JazzLights synchronization protocol, see extras/docs/WLED.md.
//
// WLED manages Wi-Fi and the LEDs. This usermod runs the JazzLights protocol over UDP multicast on WLED's network
// interface and over Bluetooth Low Energy.

// wled.h has to come first: it includes Arduino's IPAddress.h, which declares a variable named INADDR_NONE, and lwIP's
// inet.h (included by our network headers) defines INADDR_NONE as a macro.
#include "wled.h"
// WLED defines STRINGIFY without expanding macros, and our config.h defines it with expansion.
#undef STRINGIFY

#include <esp_wifi.h>

#include <cstdarg>
#include <cstdio>
#include <optional>
#include <string>

#include "jazzlights/network/esp32_nimble.h"
#include "jazzlights/network/esp32_socket_manager.h"
#include "jazzlights/network/manager.h"
#include "jazzlights/protocol/engine.h"
#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/log.h"

namespace {

// Forwards JazzLights logs to WLED's debug output, which only exists in builds with WLED_DEBUG. Fatal logs are also
// printed by LogAtLevel() itself.
void LogToWled(char level, const char* file, int line, const char* format, va_list args) {
#ifdef WLED_DEBUG
  char message[256];
  vsnprintf(message, sizeof(message), format, args);
  DEBUG_PRINTF("[JL %c] %s:%d %s\n", level, file, line, message);
#else   // WLED_DEBUG
  (void)level;
  (void)file;
  (void)line;
  (void)format;
  (void)args;
#endif  // WLED_DEBUG
}

class JazzLightsUsermod : public Usermod, private ::jazzlights::ProtocolEngine::Delegate {
 public:
  void setup() override {
    if (!enabled_) { return; }
    ::jazzlights::SetLogHandler(&LogToWled);
#if !JL_DISABLE_BLUETOOTH
    if (bluetoothEnabled_) {
      // ESP-IDF requires Wi-Fi modem sleep while Bluetooth is running, and WLED disables it by default. This runs
      // before WLED starts Wi-Fi, which then uses this value.
      noWifiSleep = false;
      // This initializes the Bluetooth stack, which needs to happen before Wi-Fi starts.
      networkManager_.Connect(::jazzlights::Esp32NimBleNetwork::Get());
    }
#endif  // !JL_DISABLE_BLUETOOTH
    engine_.SetBasePrecedence(kFollowerBasePrecedence);
    engine_.SetPrecedenceGain(kFollowerPrecedenceGain);
    engine_.SetHasNetworks(networkManager_.HasNetworks());
    engine_.SetupDeviceId(networkManager_.GetLocalDeviceId());
    started_ = true;
  }

  void loop() override {
    if (!started_) { return; }
    MaybeAddNetworkInterfaces();
    for (const ::jazzlights::ProtocolMessage& message : networkManager_.GetReceivedMessages()) {
      engine_.HandleReceivedMessage(message);
    }
    engine_.CheckLeaderAndPattern();
    networkManager_.SetMessageToSend(engine_.GetMessageToSend());
    networkManager_.RunLoop();
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) { user = root.createNestedObject("u"); }
    JsonArray status = user.createNestedArray(F("JazzLights"));
    if (!started_) {
      status.add(F("disabled"));
      return;
    }
    if (engine_.following() == ::jazzlights::NetworkType::kLeading) {
      status.add(F("leading"));
    } else {
      char followingStr[64];
      snprintf(followingStr, sizeof(followingStr), "following %s via %s, %u hops",
               engine_.currentLeader().ToString().c_str(), ::jazzlights::NetworkTypeToString(engine_.following()),
               static_cast<unsigned>(engine_.currentNumHops()));
      status.add(followingStr);
    }
    JsonArray pattern = user.createNestedArray(F("JazzLights pattern"));
    pattern.add(PatternName(engine_.GetCurrentPattern()));
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(FPSTR(kConfigKey));
    top[F("enabled")] = enabled_;
#if !JL_DISABLE_BLUETOOTH
    top[F("bluetooth")] = bluetoothEnabled_;
#endif  // !JL_DISABLE_BLUETOOTH
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[FPSTR(kConfigKey)];
    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top[F("enabled")], enabled_, true);
#if !JL_DISABLE_BLUETOOTH
    configComplete &= getJsonValue(top[F("bluetooth")], bluetoothEnabled_, true);
#endif  // !JL_DISABLE_BLUETOOTH
    return configComplete;
  }

  void appendConfigData(Print& settingsScript) override {
    settingsScript.print(F("addInfo('JazzLights:enabled',1,'requires a reboot');"));
#if !JL_DISABLE_BLUETOOTH
    settingsScript.print(F("addInfo('JazzLights:bluetooth',1,'requires a reboot, forces Wi-Fi sleep on');"));
#endif  // !JL_DISABLE_BLUETOOTH
  }

 private:
  // Follower defaults from extras/docs/WLED.md. Modes come in a later phase.
  static constexpr ::jazzlights::Precedence kFollowerBasePrecedence = 300;
  static constexpr ::jazzlights::Precedence kFollowerPrecedenceGain = 100;
  static constexpr char kConfigKey[] PROGMEM = "JazzLights";

  // Adds WLED's network interfaces to Esp32SocketManager once they exist. WLED only creates them after usermod setup.
  void MaybeAddNetworkInterfaces() {
    if (!wifiInterfaceAdded_) {
      esp_netif_t* netif = WiFi.STA.netif();
      uint8_t macAddress[6];
      if (netif != nullptr && esp_wifi_get_mac(WIFI_IF_STA, macAddress) == ESP_OK) {
        AddNetworkInterface(netif, ::jazzlights::NetworkType::kWiFi, macAddress);
        wifiInterfaceAdded_ = true;
      }
    }
#ifdef WLED_USE_ETHERNET
    if (!ethernetInterfaceAdded_) {
      esp_netif_t* netif = ETH.netif();
      uint8_t macAddress[6];
      if (netif != nullptr && ETH.macAddress(macAddress) != nullptr) {
        AddNetworkInterface(netif, ::jazzlights::NetworkType::kEthernet, macAddress);
        ethernetInterfaceAdded_ = true;
      }
    }
#endif  // WLED_USE_ETHERNET
  }

  void AddNetworkInterface(esp_netif_t* netif, ::jazzlights::NetworkType type, const uint8_t* macAddress) {
    ::jazzlights::Esp32SocketManager* socketManager = ::jazzlights::Esp32SocketManager::Get();
    socketManager->AddInterface(netif, type, ::jazzlights::NetworkDeviceId(macAddress));
    if (!socketManagerConnected_) {
      networkManager_.Connect(socketManager);
      engine_.SetHasNetworks(true);
      socketManagerConnected_ = true;
    }
  }

  // From ProtocolEngine::Delegate.
  std::string PatternName(::jazzlights::PatternBits pattern) const override {
    char name[11];
    snprintf(name, sizeof(name), "0x%08x", pattern);
    return name;
  }
  std::optional<::jazzlights::PatternBits> ForcedLeadingPattern() const override { return std::nullopt; }
  void OnPatternRestart() override {}
  void OnAcceptedUpdate() override {}
  void LogFpsReport() override {}

  bool enabled_ = true;
#if !JL_DISABLE_BLUETOOTH
  bool bluetoothEnabled_ = true;
#endif  // !JL_DISABLE_BLUETOOTH
  bool started_ = false;
  bool wifiInterfaceAdded_ = false;
#ifdef WLED_USE_ETHERNET
  bool ethernetInterfaceAdded_ = false;
#endif  // WLED_USE_ETHERNET
  bool socketManagerConnected_ = false;
  ::jazzlights::NetworkManager networkManager_;
  ::jazzlights::ProtocolEngine engine_{this};
};

JazzLightsUsermod gJazzLightsUsermod;
REGISTER_USERMOD(gJazzLightsUsermod);

}  // namespace
