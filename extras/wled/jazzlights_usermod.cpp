// WLED usermod that runs the JazzLights synchronization protocol, see extras/docs/WLED.md.
//
// WLED manages Wi-Fi and the LEDs. This usermod runs the JazzLights protocol over UDP multicast on WLED's network
// interface and over Bluetooth Low Energy, and advertises the JazzLights pattern closest to what WLED shows.

// wled.h has to come first: it includes Arduino's IPAddress.h, which declares a variable named INADDR_NONE, and lwIP's
// inet.h (included by our network headers) defines INADDR_NONE as a macro.
#include "wled.h"
// WLED defines STRINGIFY without expanding macros, and our config.h defines it with expansion.
#undef STRINGIFY

#include <esp_mac.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>

#include "jazzlights/network/esp32_nimble.h"
#include "jazzlights/network/esp32_socket_manager.h"
#include "jazzlights/network/manager.h"
#include "jazzlights/protocol/engine.h"
#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"
#include "jazzlights/util/types.h"
#include "jazzlights/wled/mapping.h"

namespace {

using ::jazzlights::PatternBits;
using ::jazzlights::Precedence;
using ::jazzlights::WLEDColor;
using ::jazzlights::WLEDSegmentState;
using ::jazzlights::WLEDTarget;

// jazzlights/wled/mapping.h can't include WLED's headers, so check its effect IDs against them here.
#define JL_CHECK_WLED_EFFECT(jlName, wledName) \
  static_assert(::jazzlights::jlName == wledName, #jlName " doesn't match " #wledName)
JL_CHECK_WLED_EFFECT(kWLEDSolid, FX_MODE_STATIC);
JL_CHECK_WLED_EFFECT(kWLEDBreath, FX_MODE_BREATH);
JL_CHECK_WLED_EFFECT(kWLEDRainbowCycle, FX_MODE_RAINBOW_CYCLE);
JL_CHECK_WLED_EFFECT(kWLEDFade, FX_MODE_FADE);
JL_CHECK_WLED_EFFECT(kWLEDTwinkle, FX_MODE_TWINKLE);
JL_CHECK_WLED_EFFECT(kWLEDDissolve, FX_MODE_DISSOLVE);
JL_CHECK_WLED_EFFECT(kWLEDDissolveRandom, FX_MODE_DISSOLVE_RANDOM);
JL_CHECK_WLED_EFFECT(kWLEDSparkle, FX_MODE_SPARKLE);
JL_CHECK_WLED_EFFECT(kWLEDFlashSparkle, FX_MODE_FLASH_SPARKLE);
JL_CHECK_WLED_EFFECT(kWLEDHyperSparkle, FX_MODE_HYPER_SPARKLE);
JL_CHECK_WLED_EFFECT(kWLEDFireworks, FX_MODE_FIREWORKS);
JL_CHECK_WLED_EFFECT(kWLEDRain, FX_MODE_RAIN);
JL_CHECK_WLED_EFFECT(kWLEDFireFlicker, FX_MODE_FIRE_FLICKER);
JL_CHECK_WLED_EFFECT(kWLEDFairyTwinkle, FX_MODE_FAIRYTWINKLE);
JL_CHECK_WLED_EFFECT(kWLEDJuggle, FX_MODE_JUGGLE);
JL_CHECK_WLED_EFFECT(kWLEDPalette, FX_MODE_PALETTE);
JL_CHECK_WLED_EFFECT(kWLEDFire2012, FX_MODE_FIRE_2012);
JL_CHECK_WLED_EFFECT(kWLEDFillNoise8, FX_MODE_FILLNOISE8);
JL_CHECK_WLED_EFFECT(kWLEDNoise1, FX_MODE_NOISE16_1);
JL_CHECK_WLED_EFFECT(kWLEDNoise2, FX_MODE_NOISE16_2);
JL_CHECK_WLED_EFFECT(kWLEDNoise3, FX_MODE_NOISE16_3);
JL_CHECK_WLED_EFFECT(kWLEDNoise4, FX_MODE_NOISE16_4);
JL_CHECK_WLED_EFFECT(kWLEDColorTwinkle, FX_MODE_COLORTWINKLE);
JL_CHECK_WLED_EFFECT(kWLEDTwinkleFox, FX_MODE_TWINKLEFOX);
JL_CHECK_WLED_EFFECT(kWLEDTwinkleCat, FX_MODE_TWINKLECAT);
JL_CHECK_WLED_EFFECT(kWLEDGlitter, FX_MODE_GLITTER);
JL_CHECK_WLED_EFFECT(kWLEDCandle, FX_MODE_CANDLE);
JL_CHECK_WLED_EFFECT(kWLEDStarburst, FX_MODE_STARBURST);
JL_CHECK_WLED_EFFECT(kWLEDExplodingFireworks, FX_MODE_EXPLODING_FIREWORKS);
JL_CHECK_WLED_EFFECT(kWLEDDrip, FX_MODE_DRIP);
JL_CHECK_WLED_EFFECT(kWLEDPlasma, FX_MODE_PLASMA);
JL_CHECK_WLED_EFFECT(kWLEDHeartbeat, FX_MODE_HEARTBEAT);
JL_CHECK_WLED_EFFECT(kWLEDCandleMulti, FX_MODE_CANDLE_MULTI);
JL_CHECK_WLED_EFFECT(kWLEDSolidGlitter, FX_MODE_SOLID_GLITTER);
JL_CHECK_WLED_EFFECT(kWLEDTwinkleUp, FX_MODE_TWINKLEUP);
JL_CHECK_WLED_EFFECT(kWLEDNoisePal, FX_MODE_NOISEPAL);
JL_CHECK_WLED_EFFECT(kWLEDSineWave, FX_MODE_SINEWAVE);
JL_CHECK_WLED_EFFECT(kWLEDPhasedNoise, FX_MODE_PHASEDNOISE);
JL_CHECK_WLED_EFFECT(kWLED2DPlasmaRotozoom, FX_MODE_2DPLASMAROTOZOOM);
JL_CHECK_WLED_EFFECT(kWLED2DBlobs, FX_MODE_2DBLOBS);
JL_CHECK_WLED_EFFECT(kWLED2DDistortionWaves, FX_MODE_2DDISTORTIONWAVES);
JL_CHECK_WLED_EFFECT(kWLED2DSoap, FX_MODE_2DSOAP);
JL_CHECK_WLED_EFFECT(kWLEDNoiseFire, FX_MODE_NOISEFIRE);
JL_CHECK_WLED_EFFECT(kWLED2DNoise, FX_MODE_2DNOISE);
JL_CHECK_WLED_EFFECT(kWLED2DFireNoise, FX_MODE_2DFIRENOISE);
JL_CHECK_WLED_EFFECT(kWLED2DDNA, FX_MODE_2DDNA);
JL_CHECK_WLED_EFFECT(kWLED2DMatrix, FX_MODE_2DMATRIX);
JL_CHECK_WLED_EFFECT(kWLED2DMetaballs, FX_MODE_2DMETABALLS);
JL_CHECK_WLED_EFFECT(kWLED2DColoredBursts, FX_MODE_2DCOLOREDBURSTS);
JL_CHECK_WLED_EFFECT(kWLED2DLissajous, FX_MODE_2DLISSAJOUS);
JL_CHECK_WLED_EFFECT(kWLED2DPlasmaBall, FX_MODE_2DPLASMABALL);
JL_CHECK_WLED_EFFECT(kWLED2DHiphotic, FX_MODE_2DHIPHOTIC);
JL_CHECK_WLED_EFFECT(kWLED2DDNASpiral, FX_MODE_2DDNASPIRAL);
JL_CHECK_WLED_EFFECT(kWLEDWaveSins, FX_MODE_WAVESINS);
JL_CHECK_WLED_EFFECT(kWLEDParticleFire, FX_MODE_PARTICLEFIRE);
JL_CHECK_WLED_EFFECT(kWLEDParticleFireworks, FX_MODE_PARTICLEFIREWORKS);
JL_CHECK_WLED_EFFECT(kWLEDParticlePerlin, FX_MODE_PARTICLEPERLIN);
JL_CHECK_WLED_EFFECT(kWLEDParticleBlobs, FX_MODE_PARTICLEBLOBS);
JL_CHECK_WLED_EFFECT(kWLEDPSDrip, FX_MODE_PSDRIP);
JL_CHECK_WLED_EFFECT(kWLEDPSStarburst, FX_MODE_PSSTARBURST);
JL_CHECK_WLED_EFFECT(kWLEDPSFire1D, FX_MODE_PSFIRE1D);
JL_CHECK_WLED_EFFECT(kWLEDColorClouds, FX_MODE_COLORCLOUDS);
#undef JL_CHECK_WLED_EFFECT

// Forwards JazzLights logs to WLED's debug output, which only exists in builds with WLED_DEBUG. Fatal logs are also
// printed by LogAtLevel() itself.
void LogToWLED(char level, const char* file, int line, const char* format, va_list args) {
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

// See "Modes and precedence" in extras/docs/WLED.md.
enum class Mode : uint8_t {
  kOff = 0,
  kFollower = 1,
  kLeader = 2,
};

const char* ModeName(Mode mode) {
  switch (mode) {
    case Mode::kOff: return "off";
    case Mode::kFollower: return "follower";
    case Mode::kLeader: return "leader";
  }
  return "unknown";
}

Precedence DefaultBasePrecedence(Mode mode) {
  switch (mode) {
    case Mode::kOff: return 0;
    case Mode::kFollower: return 300;
    case Mode::kLeader: return 20000;
  }
  return 0;
}

Precedence DefaultPrecedenceGain(Mode mode) {
  switch (mode) {
    case Mode::kOff: return 0;
    case Mode::kFollower: return 100;
    case Mode::kLeader: return 1000;
  }
  return 0;
}

// WLED describes each effect with a string such as "Breathe@!;!,!;!;01": the name, then after the @, its sliders,
// colors, palette, flags and defaults, separated by semicolons.
struct EffectMetadata {
  // Whether the effect has a palette setting.
  bool usesPalette = false;
  // Whether the effect only runs on 2D segments. WLED shows Solid instead on 1D segments.
  bool only2D = false;
};

EffectMetadata ParseEffectMetadata(uint8_t effect) {
  char data[256];
  strncpy_P(data, strip.getModeData(effect), sizeof(data) - 1);
  data[sizeof(data) - 1] = '\0';
  EffectMetadata metadata;
  const char* fields = strchr(data, '@');
  if (fields == nullptr) { return metadata; }  // Only a name, as for Solid.
  // Find the palette field, the third one, and the flags field, the fourth one.
  const char* field = fields + 1;
  for (int index = 0; field != nullptr && index < 4; index++) {
    const char* end = strchr(field, ';');
    const size_t length = end != nullptr ? static_cast<size_t>(end - field) : strlen(field);
    if (index == 2) { metadata.usesPalette = length > 0; }
    if (index == 3) {
      const std::string flags(field, length);
      metadata.only2D = flags.find('2') != std::string::npos && flags.find('0') == std::string::npos &&
                        flags.find('1') == std::string::npos;
    }
    field = end != nullptr ? end + 1 : nullptr;
  }
  return metadata;
}

WLEDColor ColorWithWhiteFolded(uint32_t color) {
  auto fold = [color](uint8_t channel) {
    return static_cast<uint8_t>(std::min<unsigned>(255, static_cast<unsigned>(channel) + W(color)));
  };
  return WLEDColor{fold(R(color)), fold(G(color)), fold(B(color))};
}

// What decides the pattern we map a segment to. When none of it changes, the mapping can't either.
struct SegmentSignature {
  uint8_t effect = 0;
  uint8_t palette = 0;
  uint32_t colors[NUM_COLORS] = {};
  bool is2D = false;

  bool operator==(const SegmentSignature& other) const {
    return effect == other.effect && palette == other.palette && is2D == other.is2D &&
           memcmp(colors, other.colors, sizeof(colors)) == 0;
  }
  bool operator!=(const SegmentSignature& other) const { return !(*this == other); }
};

#if defined(CONFIG_IDF_TARGET_ESP32) && defined(CONFIG_BTDM_CTRL_HLI) && !JL_DISABLE_BLUETOOTH
// On the classic ESP32, WLED's RMT LED driver shares the Bluetooth controller's high-level interrupt (number 25)
// through ESP-IDF's HLI dispatcher, but binds it on the core that creates the LED buses, core 1, while the controller
// is pinned to core 0. The dispatcher then runs the controller's interrupt handler on both cores at once, which
// crashes. So when Bluetooth runs, we switch WLED's digital LED buses to its parallel I2S driver before WLED creates
// them. See "Bluetooth" in extras/docs/WLED.md.
#define JL_WLED_AVOID_RMT_LED_DRIVER 1

void AvoidRmtLedDriver() {
  for (BusConfig& bus : busConfigs) {
    if (!Bus::isDigital(bus.type) || Bus::is2Pin(bus.type) || bus.driverType != BUSDRV_RMT) { continue; }
    bus.driverType = BUSDRV_PARHW;
    jll_info("Switching an LED bus from the RMT driver to the parallel I2S driver, see extras/docs/WLED.md");
  }
}

bool LedBusesUseRmtDriver() {
  for (size_t i = 0; i < BusManager::getNumBusses(); i++) {
    const Bus* bus = BusManager::getBus(i);
    if (bus != nullptr && bus->isDigital() && !bus->is2Pin() && bus->getDriverType() == BUSDRV_RMT) { return true; }
  }
  return false;
}
#else
#define JL_WLED_AVOID_RMT_LED_DRIVER 0
#endif

SegmentSignature SignatureOf(const Segment& segment) {
  SegmentSignature signature;
  signature.effect = segment.mode;
  signature.palette = segment.palette;
  memcpy(signature.colors, segment.colors, sizeof(signature.colors));
  signature.is2D = segment.is2D();
  return signature;
}

// What a follower changes on the segment, and restores when it stops following.
struct SavedSegmentState {
  uint8_t mode = 0;
  uint8_t palette = 0;
  uint32_t colors[NUM_COLORS] = {};
};

SavedSegmentState SaveSegmentState(const Segment& segment) {
  SavedSegmentState saved;
  saved.mode = segment.mode;
  saved.palette = segment.palette;
  memcpy(saved.colors, segment.colors, sizeof(saved.colors));
  return saved;
}

// Changes the segment without notifying WLED's own sync, and without saving anything to flash. stateUpdated() rather
// than colorUpdated(), which would copy WLED's global effect and palette back onto the selected segments.
void RestoreSegmentState(Segment& segment, const SavedSegmentState& saved) {
  segment.setMode(saved.mode);
  segment.setPalette(saved.palette);
  for (uint8_t slot = 0; slot < NUM_COLORS; slot++) { segment.setColor(slot, saved.colors[slot]); }
  stateUpdated(CALL_MODE_NO_NOTIFY);
}

// Applies a JazzLights pattern converted by JazzLightsToWLED(). setMode() keeps the segment's sliders.
void ApplyTarget(Segment& segment, const WLEDTarget& target) {
  segment.setMode(target.effect);
  segment.setPalette(target.palette);
  if (target.color) { segment.setColor(0, RGBW32(target.color->r, target.color->g, target.color->b, 0)); }
  if (target.blackBackground) { segment.setColor(1, BLACK); }
  stateUpdated(CALL_MODE_NO_NOTIFY);
}

// See "WLED to JazzLights (leader)" in extras/docs/WLED.md.
WLEDSegmentState ReadSegmentState(Segment& segment) {
  WLEDSegmentState state;
  state.effect = segment.mode;
  const EffectMetadata metadata = ParseEffectMetadata(segment.mode);
  state.showsSolidFallback = metadata.only2D && !segment.is2D();
  state.paletteIsDefault = segment.palette == ::jazzlights::kWLEDPaletteDefault;
  // With the Default palette, most effects draw with the segment's colors, unless they declare a default palette.
  uint8_t paletteId = segment.palette;
  if (!metadata.usesPalette) {
    paletteId = ::jazzlights::kWLEDPaletteColorsOnly;
  } else if (paletteId == ::jazzlights::kWLEDPaletteDefault) {
    const int16_t defaultPalette = extractModeDefaults(segment.mode, "pal");
    paletteId = defaultPalette > 0 ? static_cast<uint8_t>(defaultPalette) : ::jazzlights::kWLEDPaletteColorsOnly;
  }
  state.paletteId = paletteId;
  CRGBPalette16 palette;
  segment.loadPalette(palette, paletteId);
  for (size_t i = 0; i < state.palette.size(); i++) {
    state.palette[i] = WLEDColor{palette.entries[i].r, palette.entries[i].g, palette.entries[i].b};
  }
  state.color = ColorWithWhiteFolded(segment.colors[0]);
  return state;
}

class JazzLightsUsermod : public Usermod, private ::jazzlights::ProtocolEngine::Delegate {
 public:
  void setup() override {
    // Turning the usermod on needs a reboot, since Bluetooth has to start before Wi-Fi.
    if (mode_ == Mode::kOff) { return; }
    ::jazzlights::SetLogHandler(&LogToWLED);
#if JL_WLED_AVOID_RMT_LED_DRIVER
    if (bluetoothEnabled_ && LedBusesUseRmtDriver()) {
      if (!configLoaded_) {
        // WLED only calls readFromConfig(), which switches the LED driver, when its configuration has a section for
        // usermods, which a fresh install doesn't. Saving the configuration adds it, so switch on the next boot.
        jll_info("Saving WLED's configuration and rebooting to switch the LED driver, see extras/docs/WLED.md");
        serializeConfigToFS();
        doReboot = true;
        return;
      }
      // The parallel driver isn't available for some bus, so Bluetooth would crash.
      jll_error("An LED bus still uses the RMT driver, not starting Bluetooth, see extras/docs/WLED.md");
      bluetoothBlockedByLedDriver_ = true;
    }
#endif  // JL_WLED_AVOID_RMT_LED_DRIVER
#if !JL_DISABLE_BLUETOOTH
    if (bluetoothEnabled_ && !bluetoothBlockedByLedDriver_) {
      // ESP-IDF requires Wi-Fi modem sleep while Bluetooth is running, and WLED disables it by default. This runs
      // before WLED starts Wi-Fi, which then uses this value.
      noWifiSleep = false;
      // This initializes the Bluetooth stack, which needs to happen before Wi-Fi starts.
      networkManager_.Connect(::jazzlights::Esp32NimBleNetwork::Get());
      bluetoothStarted_ = true;
    }
#endif  // !JL_DISABLE_BLUETOOTH
    ApplyMode();
    engine_.SetHasNetworks(networkManager_.HasNetworks());
    // Without Bluetooth, use the Wi-Fi MAC address, so that the device ID doesn't change at every boot.
    ::jazzlights::NetworkDeviceId localDeviceId = networkManager_.GetLocalDeviceId();
    uint8_t wifiMacAddress[6];
    if (localDeviceId == ::jazzlights::NetworkDeviceId() && esp_read_mac(wifiMacAddress, ESP_MAC_WIFI_STA) == ESP_OK) {
      localDeviceId = ::jazzlights::NetworkDeviceId(wifiMacAddress);
    }
    engine_.SetupDeviceId(localDeviceId);
    started_ = true;
  }

  void loop() override {
    if (!started_) { return; }
#if JL_WLED_AVOID_RMT_LED_DRIVER
    // Saving WLED's LED settings recreates the buses later in this iteration of WLED's loop.
    if (bluetoothStarted_ && doInitBusses) { AvoidRmtLedDriver(); }
#endif  // JL_WLED_AVOID_RMT_LED_DRIVER
    const uint32_t nowMs = millis();
    UpdateNetworkInterfaces();
    UpdateBluetoothPause(nowMs);
    if (updating_) {
      DiscardReceivedMessages();
      return;
    }
    UpdatePresence(nowMs);
    if (presence_ == Presence::kAbsent) {
      DiscardReceivedMessages();
      return;
    }
    UpdateLeadingPattern(nowMs);
    for (const ::jazzlights::ProtocolMessage& message : networkManager_.GetReceivedMessages()) {
      engine_.HandleReceivedMessage(message);
    }
    engine_.CheckLeaderAndPattern();
    UpdateFollowing(nowMs);
    networkManager_.SetMessageToSend(engine_.GetMessageToSend(), sendAsap_);
    sendAsap_ = false;
    networkManager_.RunLoop();
  }

  void onStateChange(uint8_t callMode) override {
    // Changes made by a person count as user input. Playlists also report CALL_MODE_DIRECT_CHANGE, so we tell them
    // apart with currentPlaylist. See "Which WLED changes count as user input" in extras/docs/WLED.md.
    const bool fromPerson =
        callMode == CALL_MODE_DIRECT_CHANGE || callMode == CALL_MODE_BUTTON || callMode == CALL_MODE_BUTTON_PRESET;
    if (fromPerson && currentPlaylist < 0) { userInputTimeMs_ = millis(); }
  }

  void onUpdateBegin(bool updating) override {
    // Free memory and radio time for the firmware download, and resume if the update fails.
    if (!started_ || updating == updating_) { return; }
    updating_ = updating;
    if (updating) {
      PauseNetworks();
    } else if (presence_ != Presence::kAbsent) {
      ResumeNetworks();
    }
    UpdateBluetoothPause(millis());
  }

  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"];
    if (user.isNull()) { user = root.createNestedObject("u"); }
    JsonArray status = user.createNestedArray(F("JazzLights"));
    if (!started_) {
      status.add(mode_ == Mode::kOff ? F("off") : F("reboot to start"));
      return;
    }
    char statusStr[96];
    if (presence_ == Presence::kAbsent) {
      snprintf(statusStr, sizeof(statusStr), "%s, not participating while off", ModeName(mode_));
    } else if (presence_ == Presence::kLeaving) {
      snprintf(statusStr, sizeof(statusStr), "%s, leaving", ModeName(mode_));
    } else if (engine_.following() == ::jazzlights::NetworkType::kLeading) {
      snprintf(statusStr, sizeof(statusStr), "%s, leading", ModeName(mode_));
    } else {
      snprintf(statusStr, sizeof(statusStr), "%s, following %s via %s, %u hops", ModeName(mode_),
               engine_.currentLeader().ToString().c_str(), ::jazzlights::NetworkTypeToString(engine_.following()),
               static_cast<unsigned>(engine_.currentNumHops()));
    }
    status.add(statusStr);
#if !JL_DISABLE_BLUETOOTH
    if (bluetoothBlockedByLedDriver_) {
      JsonArray bluetooth = user.createNestedArray(F("JazzLights Bluetooth"));
      bluetooth.add(F("off: an LED output uses the RMT driver"));
    }
#endif  // !JL_DISABLE_BLUETOOTH
    JsonArray pattern = user.createNestedArray(F("JazzLights pattern"));
    pattern.add(PatternName(engine_.GetCurrentPattern()));
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(FPSTR(kConfigKey));
    top[F("mode")] = static_cast<int>(mode_);
    top[F("base precedence")] = basePrecedence_;
    top[F("precedence gain")] = precedenceGain_;
    top[F("segment")] = segmentId_;
#if !JL_DISABLE_BLUETOOTH
    top[F("bluetooth")] = bluetoothEnabled_;
#endif  // !JL_DISABLE_BLUETOOTH
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[FPSTR(kConfigKey)];
    bool configComplete = !top.isNull();
    const Mode previousMode = mode_;
    const Precedence previousBase = basePrecedence_;
    const Precedence previousGain = precedenceGain_;
    int mode = static_cast<int>(mode_);
    configComplete &= getJsonValue(top[F("mode")], mode, static_cast<int>(kDefaultMode));
    mode_ = mode >= static_cast<int>(Mode::kOff) && mode <= static_cast<int>(Mode::kLeader) ? static_cast<Mode>(mode)
                                                                                            : kDefaultMode;
    configComplete &= getJsonValue(top[F("base precedence")], basePrecedence_, DefaultBasePrecedence(mode_));
    configComplete &= getJsonValue(top[F("precedence gain")], precedenceGain_, DefaultPrecedenceGain(mode_));
    configComplete &= getJsonValue(top[F("segment")], segmentId_, kMainSegment);
#if !JL_DISABLE_BLUETOOTH
    configComplete &= getJsonValue(top[F("bluetooth")], bluetoothEnabled_, true);
#endif  // !JL_DISABLE_BLUETOOTH
    // The precedences are prefilled from the mode: switching modes without editing them picks the new mode's defaults.
    if (configLoaded_ && mode_ != previousMode && basePrecedence_ == previousBase && precedenceGain_ == previousGain) {
      basePrecedence_ = DefaultBasePrecedence(mode_);
      precedenceGain_ = DefaultPrecedenceGain(mode_);
    }
#if JL_WLED_AVOID_RMT_LED_DRIVER
    // At boot, WLED reads usermod settings after its LED settings and before it creates the buses.
    if (!configLoaded_ && mode_ != Mode::kOff && bluetoothEnabled_) { AvoidRmtLedDriver(); }
#endif  // JL_WLED_AVOID_RMT_LED_DRIVER
    configLoaded_ = true;
    if (started_) { ApplyMode(); }
    return configComplete;
  }

  void appendConfigData(Print& settingsScript) override {
    settingsScript.print(F("dd=addDropdown('JazzLights','mode');"));
    settingsScript.print(F("addOption(dd,'Off',0);addOption(dd,'Follower',1);addOption(dd,'Leader',2);"));
    settingsScript.print(F("addInfo('JazzLights:mode',1,'turning on from Off requires a reboot');"));
    settingsScript.print(F("addInfo('JazzLights:base precedence',1,'reset to the mode default when changing mode');"));
    settingsScript.print(F("addInfo('JazzLights:segment',1,'255 for the main segment');"));
#if !JL_DISABLE_BLUETOOTH
    settingsScript.print(F("addInfo('JazzLights:bluetooth',1,'requires a reboot, forces Wi-Fi sleep on');"));
#endif  // !JL_DISABLE_BLUETOOTH
  }

 private:
  // The device takes part in the protocol while WLED is on. When it turns off, the device first advertises precedence 0
  // for kLeavingDurationMs, so devices following it elect another leader right away, then stops listening and sending.
  // See "When WLED is turned off" in extras/docs/WLED.md.
  enum class Presence : uint8_t {
    kPresent,
    kLeaving,
    kAbsent,
  };

  static constexpr Mode kDefaultMode = Mode::kFollower;
  static constexpr uint8_t kMainSegment = 255;
  static constexpr uint32_t kLeavingDurationMs = 10000;
  // How long the mapped pattern has to stay the same before we advertise it, so that dragging the color wheel doesn't
  // restart other devices' effects dozens of times per second.
  static constexpr uint32_t kMappingDebounceMs = 250;
  // How long after a person's change we may see it in the segment.
  static constexpr uint32_t kUserInputWindowMs = 1000;
  // While WLED's access point is on without clients, Bluetooth runs kAccessPointBluetoothOnMs out of every
  // kAccessPointBluetoothCycleMs.
  static constexpr uint32_t kAccessPointBluetoothCycleMs = 2000;
  static constexpr uint32_t kAccessPointBluetoothOnMs = 1000;
  static constexpr uint32_t kAccessPointCheckIntervalMs = 100;
  static constexpr char kConfigKey[] PROGMEM = "JazzLights";

  Segment& FollowedSegment() {
    if (segmentId_ != kMainSegment && segmentId_ < strip.getSegmentsNum()) { return strip.getSegment(segmentId_); }
    return strip.getMainSegment();
  }

  bool IsWLEDOn() {
    if (mode_ == Mode::kOff || bri == 0) { return false; }
    const Segment& segment = FollowedSegment();
    return segment.on && segment.isActive();
  }

  void ApplyMode() {
    if (presence_ != Presence::kPresent) { return; }
    if (engine_.UpdatePrecedence(basePrecedence_, precedenceGain_)) { sendAsap_ = true; }
    // A leader's user input only adds the gain, so that a Core2 in override mode can always take over. See "What a
    // person's change does in each mode" in extras/docs/WLED.md.
    engine_.SetUserInputKeepsLeading(mode_ != Mode::kLeader);
  }

  void UpdatePresence(uint32_t nowMs) {
    const bool on = IsWLEDOn();
    switch (presence_) {
      case Presence::kPresent:
        if (!on) {
          jll_info("WLED is off, leaving the JazzLights protocol");
          presence_ = Presence::kLeaving;
          leavingStartMs_ = nowMs;
          engine_.UpdatePrecedence(0, 0);
          sendAsap_ = true;
        }
        break;
      case Presence::kLeaving:
        if (on) {
          Rejoin();
        } else if (nowMs - leavingStartMs_ >= kLeavingDurationMs) {
          jll_info("Stopped listening and sending");
          presence_ = Presence::kAbsent;
          PauseNetworks();
        }
        break;
      case Presence::kAbsent:
        if (on) {
          ResumeNetworks();
          Rejoin();
        }
        break;
    }
  }

  void Rejoin() {
    jll_info("WLED is on, joining the JazzLights protocol");
    presence_ = Presence::kPresent;
    ApplyMode();
    // Turning WLED back on counts as user input for a leader.
    if (mode_ == Mode::kLeader) { engine_.RecordUserInput(); }
    sendAsap_ = true;
  }

  // Bluetooth is paused by UpdateBluetoothPause().
  void PauseNetworks() {
    if (socketManagerConnected_) { ::jazzlights::Esp32SocketManager::Get()->DisableSending(); }
  }

  void ResumeNetworks() {
    // Esp32SocketManager resumes sending at the next SetMessageToSend().
  }

  // Pauses Bluetooth while we don't take part in the protocol, and gives Wi-Fi radio time while WLED's access point is
  // on. Wi-Fi and Bluetooth share the radio, and phones failed to join the access point while we scanned for Bluetooth
  // almost continuously. See "Bluetooth" in extras/docs/WLED.md.
  void UpdateBluetoothPause(uint32_t nowMs) {
#if !JL_DISABLE_BLUETOOTH
    if (!bluetoothStarted_) { return; }
    if (nowMs - lastAccessPointCheckMs_ >= kAccessPointCheckIntervalMs) {
      lastAccessPointCheckMs_ = nowMs;
      accessPointHasClients_ = apActive && WiFi.softAPgetStationNum() > 0;
    }
    bool paused = false;
    if (updating_ || presence_ == Presence::kAbsent) {
      paused = true;
    } else if (apActive) {
      // Pause entirely while someone uses the access point, and half of the time otherwise, so that phones can join it.
      paused = accessPointHasClients_ || nowMs % kAccessPointBluetoothCycleMs >= kAccessPointBluetoothOnMs;
    }
    if (paused == bluetoothPaused_) { return; }
    bluetoothPaused_ = paused;
    ::jazzlights::Esp32NimBleNetwork::Get()->SetPaused(paused);
#else   // !JL_DISABLE_BLUETOOTH
    (void)nowMs;
#endif  // !JL_DISABLE_BLUETOOTH
  }

  void DiscardReceivedMessages() { (void)networkManager_.GetReceivedMessages(); }

  // Maps the followed segment's state to the pattern we advertise when we lead. See "WLED to JazzLights (leader)" in
  // extras/docs/WLED.md.
  void UpdateLeadingPattern(uint32_t nowMs) {
    // While we show the followed pattern, the segment shows what we applied, not what WLED was set to.
    if (savedState_) { return; }
    Segment& segment = FollowedSegment();
    const SegmentSignature signature = SignatureOf(segment);
    if (!lastSignature_ || signature != *lastSignature_) {
      lastSignature_ = signature;
      const PatternBits mapped = ::jazzlights::WLEDToJazzLights(ReadSegmentState(segment));
      if (mapped != targetPattern_) {
        targetPattern_ = mapped;
        targetSinceMs_ = nowMs;
      }
      if (userInputTimeMs_ && nowMs - *userInputTimeMs_ < kUserInputWindowMs) {
        engine_.RecordUserInput();
        userInputTimeMs_.reset();
      }
    }
    if (userInputTimeMs_ && nowMs - *userInputTimeMs_ >= kUserInputWindowMs) {
      // A change that doesn't affect the pattern, such as brightness, doesn't count.
      userInputTimeMs_.reset();
    }
    if (!appliedPattern_ || (targetPattern_ != *appliedPattern_ && nowMs - targetSinceMs_ >= kMappingDebounceMs)) {
      appliedPattern_ = targetPattern_;
      // Each change gets new random bits, which vary the effect's look, but a held pattern keeps them. See "Randomizing
      // the unused bits" in extras/docs/WLED.md.
      engine_.SetLeadingPattern(::jazzlights::RandomizePatternBits(targetPattern_));
      sendAsap_ = true;
    }
  }

  // Shows the pattern we follow on the segment, or restores WLED's own state when we stop following. See "JazzLights
  // to WLED (follower)" in extras/docs/WLED.md.
  void UpdateFollowing(uint32_t nowMs) {
    Segment& segment = FollowedSegment();
    if (savedState_ && userInputTimeMs_) {
      if (SignatureOf(segment) != appliedSignature_) {
        // A person changed WLED while it showed the followed pattern. Their change is what we go back to. In follower
        // mode, the user input also pauses following.
        savedState_ = SaveSegmentState(segment);
        appliedSignature_ = SignatureOf(segment);
        engine_.RecordUserInput();
        userInputTimeMs_.reset();
      } else if (nowMs - *userInputTimeMs_ >= kUserInputWindowMs) {
        userInputTimeMs_.reset();
      }
    }
    const bool following =
        presence_ == Presence::kPresent && engine_.following() != ::jazzlights::NetworkType::kLeading;
    if (!following) {
      if (savedState_) {
        RestoreSegmentState(segment, *savedState_);
        savedState_.reset();
        // Map the restored state again.
        lastSignature_.reset();
      }
      shownPattern_.reset();
      return;
    }
    // Like JazzLights devices, switch to the next pattern at the end of the current one, without waiting for the leader
    // to tell us.
    const ::jazzlights::Microseconds currentTime = ::jazzlights::TimeMicros();
    ::jazzlights::Microseconds patternStartTime = engine_.currentPatternStartTime();
    PatternBits pattern = engine_.GetCurrentPattern();
    if (currentTime - patternStartTime > ::jazzlights::kEffectDuration) {
      pattern = engine_.GetNextPattern();
      patternStartTime += ::jazzlights::kEffectDuration;
    }
    // A pattern restarting every 10 seconds keeps going, so that WLED's effects don't jump.
    if (shownPattern_ && *shownPattern_ == pattern) { return; }
    shownPattern_ = pattern;
    const std::optional<WLEDTarget> target = ::jazzlights::JazzLightsToWLED(pattern, segment.is2D());
    // Test and calibration patterns keep what the segment shows.
    if (!target) { return; }
    if (!savedState_) { savedState_ = SaveSegmentState(segment); }
    ApplyTarget(segment, *target);
    appliedSignature_ = SignatureOf(segment);
    // Make WLED's effect clock count from the pattern's start, so that WLED followers show time-based effects in sync.
    const uint32_t elapsedMs = static_cast<uint32_t>((currentTime - patternStartTime) / 1000);
    strip.timebase = elapsedMs - millis();
  }

  // Keeps Esp32SocketManager in sync with WLED's network interfaces. We only register an interface while it has an
  // address: when WLED reconnects, it stops Wi-Fi with WiFi.mode(WIFI_MODE_NULL), which destroys the interfaces and
  // creates new ones, and Esp32SocketManager must not keep a pointer to a destroyed interface.
  void UpdateNetworkInterfaces() {
    uint8_t macAddress[6];
    esp_netif_t* wifiNetif = WiFi.STA.hasIP() ? WiFi.STA.netif() : nullptr;
    if (wifiNetif != nullptr && esp_wifi_get_mac(WIFI_IF_STA, macAddress) != ESP_OK) { wifiNetif = nullptr; }
    UpdateNetworkInterface(&wifiNetif_, wifiNetif, ::jazzlights::NetworkType::kWiFi, macAddress);
#ifdef WLED_USE_ETHERNET
    esp_netif_t* ethernetNetif = ETH.hasIP() ? ETH.netif() : nullptr;
    if (ethernetNetif != nullptr && ETH.macAddress(macAddress) == nullptr) { ethernetNetif = nullptr; }
    UpdateNetworkInterface(&ethernetNetif_, ethernetNetif, ::jazzlights::NetworkType::kEthernet, macAddress);
#endif  // WLED_USE_ETHERNET
  }

  void UpdateNetworkInterface(esp_netif_t** registered, esp_netif_t* current, ::jazzlights::NetworkType type,
                              const uint8_t* macAddress) {
    if (current == *registered) { return; }
    ::jazzlights::Esp32SocketManager* socketManager = ::jazzlights::Esp32SocketManager::Get();
    if (*registered != nullptr) { socketManager->RemoveInterface(*registered); }
    *registered = current;
    if (current == nullptr) { return; }
    socketManager->AddInterface(current, type, ::jazzlights::NetworkDeviceId(macAddress));
    if (!socketManagerConnected_) {
      networkManager_.Connect(socketManager);
      engine_.SetHasNetworks(true);
      socketManagerConnected_ = true;
    }
  }

  // From ProtocolEngine::Delegate.
  std::string PatternName(PatternBits pattern) const override { return ::jazzlights::DecodePattern(pattern).name; }
  std::optional<PatternBits> ForcedLeadingPattern() const override { return std::nullopt; }
  void OnPatternRestart() override {}
  void OnAcceptedUpdate() override {}
  void LogFpsReport() override {}

  // Settings.
  Mode mode_ = kDefaultMode;
  Precedence basePrecedence_ = DefaultBasePrecedence(kDefaultMode);
  Precedence precedenceGain_ = DefaultPrecedenceGain(kDefaultMode);
  uint8_t segmentId_ = kMainSegment;
#if !JL_DISABLE_BLUETOOTH
  bool bluetoothEnabled_ = true;
  bool bluetoothStarted_ = false;
  // Set when Bluetooth didn't start because an LED bus uses WLED's RMT driver, see AvoidRmtLedDriver().
  bool bluetoothBlockedByLedDriver_ = false;
  bool bluetoothPaused_ = false;
  bool accessPointHasClients_ = false;
  uint32_t lastAccessPointCheckMs_ = 0;
#endif  // !JL_DISABLE_BLUETOOTH
  bool configLoaded_ = false;

  bool started_ = false;
  bool updating_ = false;
  bool sendAsap_ = false;
  Presence presence_ = Presence::kPresent;
  uint32_t leavingStartMs_ = 0;

  std::optional<SegmentSignature> lastSignature_;
  PatternBits targetPattern_ = 0;
  uint32_t targetSinceMs_ = 0;
  std::optional<PatternBits> appliedPattern_;
  std::optional<uint32_t> userInputTimeMs_;

  // While we show the followed pattern: WLED's own state to restore, and the segment as we last left it.
  std::optional<SavedSegmentState> savedState_;
  SegmentSignature appliedSignature_;
  std::optional<PatternBits> shownPattern_;

  // The interfaces registered with Esp32SocketManager.
  esp_netif_t* wifiNetif_ = nullptr;
#ifdef WLED_USE_ETHERNET
  esp_netif_t* ethernetNetif_ = nullptr;
#endif  // WLED_USE_ETHERNET
  bool socketManagerConnected_ = false;
  ::jazzlights::NetworkManager networkManager_;
  ::jazzlights::ProtocolEngine engine_{this};
};

JazzLightsUsermod gJazzLightsUsermod;
REGISTER_USERMOD(gJazzLightsUsermod);

}  // namespace
