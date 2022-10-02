#include "jazzlights/vest.h"

#include "jazzlights/config.h"

#if WEARABLE

#ifndef JL_FASTLED_ASYNC
#ifdef ESP32
#define JL_FASTLED_ASYNC 1
#else  // ESP32
#define JL_FASTLED_ASYNC 0
#endif  // ESP32
#endif  // JL_FASTLED_ASYNC

#include <memory>
#if JL_FASTLED_ASYNC
#include <mutex>
#endif  // JL_FASTLED_ASYNC

#include "jazzlights/board.h"
#include "jazzlights/button.h"
#include "jazzlights/core2.h"
#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/arduino_ethernet.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/player.h"

#if CORE2AWS
#define LED_PIN 32
#elif defined(ESP32)
#define LED_PIN 26
#define LED_PIN2 32
#elif defined(ESP8266)
#define LED_PIN 5
#else
#error "Unexpected board"
#endif

namespace jazzlights {

#if JL_FASTLED_ASYNC
static std::mutex gLockedLedMutex;
#endif  // JL_FASTLED_ASYNC

Network* GetWiFiNetwork() { return ArduinoEspWiFiNetwork::get(); }

class FastLedRenderer : public Renderer {
 public:
  explicit FastLedRenderer(size_t numLeds) : ledMemorySize_(numLeds * sizeof(CRGB)) {
    ledsPlayer_ = reinterpret_cast<CRGB*>(calloc(ledMemorySize_, 1));
    ledsLocked_ = reinterpret_cast<CRGB*>(calloc(ledMemorySize_, 1));
    ledsFastLed_ = reinterpret_cast<CRGB*>(calloc(ledMemorySize_, 1));
    if (ledsPlayer_ == nullptr || ledsLocked_ == nullptr || ledsFastLed_ == nullptr) {
      jll_fatal("Failed to allocate %zu*%zu", numLeds, sizeof(CRGB));
    }
  }
  ~FastLedRenderer() {
    free(ledsPlayer_);
    free(ledsLocked_);
    free(ledsFastLed_);
  }

  // 4 wires with specified data rate.
  template <ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint32_t SPI_DATA_RATE>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    std::unique_ptr<FastLedRenderer> r = std::unique_ptr<FastLedRenderer>(new FastLedRenderer(numLeds));
    r->ledController_ =
        &FastLED.addLeds<CHIPSET, DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE>(r->ledsFastLed_, numLeds);
    return r;
  }

  // 4 wires with default data rate.
  template <ESPIChipsets CHIPSET, uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    std::unique_ptr<FastLedRenderer> r = std::unique_ptr<FastLedRenderer>(new FastLedRenderer(numLeds));
    r->ledController_ = &FastLED.addLeds<CHIPSET, DATA_PIN, CLOCK_PIN, RGB_ORDER>(r->ledsFastLed_, numLeds);
    return r;
  }

  // 3 wires.
  template <template <uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
  static std::unique_ptr<FastLedRenderer> Create(size_t numLeds) {
    std::unique_ptr<FastLedRenderer> r = std::unique_ptr<FastLedRenderer>(new FastLedRenderer(numLeds));
    r->ledController_ = &FastLED.addLeds<CHIPSET, DATA_PIN, RGB_ORDER>(r->ledsFastLed_, numLeds);
    return r;
  }

  void render(InputStream<Color>& colors) override {
    size_t i = 0;
    for (auto color : colors) {
      RgbaColor rgba = color.asRgba();
      ledsPlayer_[i] = CRGB(rgba.red, rgba.green, rgba.blue);
      i++;
    }
  }

  uint32_t GetPowerAtFullBrightness() const {
    return calculate_unscaled_power_mW(ledController_->leds(), ledController_->size());
  }

  void copyLedsFromPlayerToLocked() {
    freshLedLockedDataAvailable_ = true;
    memcpy(ledsLocked_, ledsPlayer_, ledMemorySize_);
  }
  bool copyLedsFromLockedToFastLed() {
    const bool freshData = freshLedLockedDataAvailable_;
    if (freshData) {
      memcpy(ledsFastLed_, ledsLocked_, ledMemorySize_);
      freshLedLockedDataAvailable_ = false;
    }
    return freshData;
  }

  void sendToLeds(uint8_t brightness = 255) { ledController_->showLeds(brightness); }

 private:
  size_t ledMemorySize_;
  CRGB* ledsPlayer_;
  bool freshLedLockedDataAvailable_ = false;
  CRGB* ledsLocked_;  // Protected by gLockedLedMutex.
  CRGB* ledsFastLed_;
  CLEDController* ledController_;
};

#if JAZZLIGHTS_ARDUINO_ETHERNET
NetworkDeviceId GetEthernetDeviceId() {
  NetworkDeviceId deviceId = GetWiFiNetwork()->getLocalDeviceId();
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
ArduinoEthernetNetwork ethernetNetwork(GetEthernetDeviceId());
#endif  // JAZZLIGHTS_ARDUINO_ETHERNET
Player player;

std::unique_ptr<FastLedRenderer> mainVestRenderer;
#if LEDNUM2
std::unique_ptr<FastLedRenderer> mainVestRenderer2;
#endif  // LEDNUM2

void sendLedsToFastLed() {
  bool shouldWrite;
#if LEDNUM2
  bool shouldWrite2;
#endif  // LEDNUM2
  {
#if JL_FASTLED_ASYNC
    const std::lock_guard<std::mutex> lock(gLockedLedMutex);
#endif  // JL_FASTLED_ASYNC
    shouldWrite = mainVestRenderer->copyLedsFromLockedToFastLed();
#if LEDNUM2
    shouldWrite2 = mainVestRenderer2->copyLedsFromLockedToFastLed();
#endif  // LEDNUM2
  }
  SAVE_COUNT_POINT(LedPrintLoop);
#if LEDNUM2
  if (!shouldWrite && !shouldWrite2) { return; }
#else   // LEDNUM2
  if (!shouldWrite) { return; }
#endif  // LEDNUM2

  uint32_t brightness = getBrightness();  // May be reduced if this exceeds our power budget with the current pattern

#if MAX_MILLIWATTS
  uint32_t powerAtFullBrightness = mainVestRenderer->GetPowerAtFullBrightness();
#if LEDNUM2
  powerAtFullBrightness += mainVestRenderer->GetPowerAtFullBrightness2();
#endif  // LEDNUM2
  const uint32_t powerAtDesiredBrightness =
      powerAtFullBrightness * brightness / 256;  // Forecast power at our current desired brightness
  player.powerLimited = (powerAtDesiredBrightness > MAX_MILLIWATTS);
  if (player.powerLimited) { brightness = brightness * MAX_MILLIWATTS / powerAtDesiredBrightness; }

  jll_debug("pf%6u    pd%5u    bu%4u    bs%4u    mW%5u    mA%5u%s", powerAtFullBrightness,
            powerAtDesiredBrightness,     // Full-brightness power, desired-brightness power
            getBrightness(), brightness,  // Desired and selected brightness
            powerAtFullBrightness * brightness / 256,
            powerAtFullBrightness * brightness / 256 / 5,  // Selected power & current
            player.powerLimited ? " (limited)" : "");
#endif  // MAX_MILLIWATTS
  SAVE_TIME_POINT(Brightness);

  ledWriteStart();
  SAVE_COUNT_POINT(LedPrintSend);
  if (shouldWrite) { mainVestRenderer->sendToLeds(brightness); }
  SAVE_TIME_POINT(MainLED);
#if LEDNUM2
  if (shouldWrite2) { mainVestRenderer2->sendToLeds(brightness); }
#endif  // LEDNUM2
  ledWriteEnd();
  SAVE_TIME_POINT(SecondLED);
}

#if JL_FASTLED_ASYNC
TaskHandle_t gFastLedTaskHandle = nullptr;
// Index 0 is normally reserved by FreeRTOS for stream and message buffers. However, the default precompiled FreeRTOS
// kernel for arduino/esp-idf only allows a single notification, so we use index 0 here. We don't use stream or message
// buffers on this specific task so we should be fine.
constexpr UBaseType_t kFastLedNotificationIndex = 0;
static_assert(kFastLedNotificationIndex < configTASK_NOTIFICATION_ARRAY_ENTRIES, "index too high");
void fastLedTaskFunction(void* /*parameters*/) {
  while (true) {
    sendLedsToFastLed();
    // Block this task until we are notified that there is new data to write.
    (void)ulTaskGenericNotifyTake(kFastLedNotificationIndex, pdTRUE, portMAX_DELAY);
  }
}
#endif  // JL_FASTLED_ASYNC

void vestSetup(void) {
  Milliseconds currentTime = timeMillis();
  Serial.begin(115200);
#if CORE2AWS
  core2SetupStart(player, currentTime);
#else   // CORE2AWS
  setupButtons();
#endif  // CORE2AWS

#if GECKO_SCALES
  // Note to self for future reference: we were able to get the 2018 Gecko Robot scales to light
  // up correctly with the M5Stack ATOM Matrix without any level shifters.
  // Wiring of the 2018 Gecko Robot scales: (1) = Data, (2) = Ground, (3) = Clock, (4) = 12VDC
  // if we number the wires on the male connector (assuming notch is up top):
  // (1)  (4)
  // (2)  (3)
  // conversely on the female connector (also assuming notch is up top):
  // (4)  (1)
  // (3)  (2)
  // M5 black wire = Ground = scale (2) = scale pigtail black (also connect 12VDC power supply ground here)
  // M5 red wire = 5VDC power supply - not connected to scale pigtail
  // M5 yellow wire = G26 = Data = scale (1) = scale pigtail yellow/green
  // M5 white wire = G32 = Clock = scale (3) = scale pigtail blue in some cases, brown in others
  // 12VDC power supply = scale (4) = scale pigtail brown in some cases, blue in others
  // IMPORTANT: it appears that on some pigtails brown and blue are inverted.
  // Separately, on the two-wire pigtails for power injection, blue is 12VDC and brown is Ground.
  // IMPORTANT: the two-wire pigtail is unfortunately reversible, and needs to be plugged in such
  // that the YL inscription is on the male end of the three-way power-injection splitter.
  constexpr uint32_t kSpiSpeed = DATA_RATE_MHZ(2);
  // The FastLED default for WS2801 is 1MHz. Empirically without long runs of wire it appeared that 2MHz and 3MHz both
  // worked while 4MHz caused visible glitches. We chose 2MHz because that allows us to run the robot at 100FPS while
  // 1MHz doesn't. Empirical data indicates that for the 378 LEDs in the robot (310 on side and 68 in head), 1MHz
  // takes 10.7ms to render while 2MHz takes 5.7ms.
  mainVestRenderer =
      std::move(FastLedRenderer::Create</*CHIPSET=*/WS2801, /*DATA_PIN=*/26, /*CLOCK_PIN=*/32, /*RGB_ORDER=*/GBR,
                                        /*SPI_SPEED=*/kSpiSpeed>(LEDNUM));
#elif CABOOSE_LIGHTS
  // The caboose uses Total Control Lighing from Cool Neon.
  // https://www.coolneon.com/tutorials-2/total-control-lighting/total-control-lighting-faq/
  // Atom Matrix Black = Ground = TCL Blue
  // Atom Matrix Red = 5VDC = TCL Red
  // Atom Matrix Yellow (G26) = Data = TCL Green
  // Atom Matrix White (G32) = Clock = TCL Yellow
  mainVestRenderer = std::move(FastLedRenderer::Create</*CHIPSET=*/P9813, /*DATA_PIN=*/26, /*CLOCK_PIN=*/32,
                                                       /*RGB_ORDER=*/RGB>(LEDNUM));
#elif IS_STAFF
  mainVestRenderer = std::move(FastLedRenderer::Create<WS2811, LED_PIN, RGB>(LEDNUM));
#elif IS_ROPELIGHT
  mainVestRenderer = std::move(FastLedRenderer::Create<WS2811, LED_PIN, BRG>(LEDNUM));
#else  // Vest.
  mainVestRenderer = std::move(FastLedRenderer::Create<WS2812B, LED_PIN, GRB>(LEDNUM));
#endif
#if LEDNUM2
#if GECKO_SCALES
  mainVestRenderer2 =
      std::move(FastLedRenderer::Create</*CHIPSET=*/WS2801, /*DATA_PIN=*/21, /*CLOCK_PIN=*/32, /*RGB_ORDER=*/GBR,
                                        /*SPI_SPEED=*/kSpiSpeed>(LEDNUM2));
#else   // GECKO_SCALES
  mainVestRenderer2 = std::move(FastLedRenderer::Create<WS2812B, LED_PIN2, GRB>(LEDNUM2));
#endif  // GECKO_SCALES
#endif  // LEDNUM2

  player.addStrand(*GetLayout(), *mainVestRenderer);
#if LEDNUM2
  player.addStrand(*GetLayout2(), *mainVestRenderer2);
#endif  // LEDNUM2
#if IS_ROBOT
  player.setBasePrecedence(20000);
  player.setPrecedenceGain(5000);
#elif GECKO_FOOT
  player.setBasePrecedence(2500);
  player.setPrecedenceGain(1000);
#elif FAIRY_WAND || IS_STAFF || IS_CAPTAIN_HAT
  player.setBasePrecedence(500);
  player.setPrecedenceGain(100);
#else
  player.setBasePrecedence(1000);
  player.setPrecedenceGain(1000);
#endif

#if ESP32_BLE
  player.connect(Esp32BleNetwork::get());
#endif  // ESP32_BLE
  player.connect(GetWiFiNetwork());
#if JAZZLIGHTS_ARDUINO_ETHERNET
  player.connect(&ethernetNetwork);
#endif  // JAZZLIGHTS_ARDUINO_ETHERNET
  player.begin(currentTime);

#if CORE2AWS
  core2SetupEnd(player, currentTime);
#endif  // CORE2AWS

#if JL_FASTLED_ASYNC
  // The Arduino loop is pinned to core 1 so we pin FastLED writes to core 0.
  BaseType_t ret = xTaskCreatePinnedToCore(fastLedTaskFunction, "FastLED", configMINIMAL_STACK_SIZE + 400,
                                           /*parameters=*/nullptr,
                                           /*priority=*/30, &gFastLedTaskHandle, /*coreID=*/0);
  if (ret != pdPASS) { jll_fatal("Failed to create FastLED task"); }
#endif  // JL_FASTLED_ASYNC
}

void vestLoop(void) {
  SAVE_TIME_POINT(LoopStart);
  Milliseconds currentTime = timeMillis();
#if CORE2AWS
  core2Loop(player, currentTime);
#else  // CORE2AWS
  SAVE_TIME_POINT(Core2);
  // Read, debounce, and process the buttons, and perform actions based on button state.
  doButtons(player, *GetWiFiNetwork(),
#if ESP32_BLE
            *Esp32BleNetwork::get(),
#endif  // ESP32_BLE
            currentTime);
#endif  // CORE2AWS
  SAVE_TIME_POINT(Buttons);

#if ESP32_BLE
  Esp32BleNetwork::get()->runLoop(currentTime);
#endif  // ESP32_BLE
  SAVE_TIME_POINT(Bluetooth);

  const bool shouldRender = player.render(currentTime);
  SAVE_TIME_POINT(Player);
  if (!shouldRender) { return; }
  {
#if JL_FASTLED_ASYNC
    const std::lock_guard<std::mutex> lock(gLockedLedMutex);
#endif  // JL_FASTLED_ASYNC
    mainVestRenderer->copyLedsFromPlayerToLocked();
#if LEDNUM2
    mainVestRenderer2->copyLedsFromPlayerToLocked();
#endif  // LEDNUM2
  }

#if JL_FASTLED_ASYNC
  // Notify the FastLED task that there is new data to write.
  (void)xTaskGenericNotify(gFastLedTaskHandle, kFastLedNotificationIndex,
                           /*notification_value=*/0, eNoAction, /*previousNotificationValue=*/nullptr);
  vTaskDelay(1);  // Yield.
#else             // !JL_FASTLED_ASYNC
  sendLedsToFastLed();
#endif            // !JL_FASTLED_ASYNC
}

std::string wifiStatus(Milliseconds currentTime) { return GetWiFiNetwork()->getStatusStr(currentTime); }

std::string bleStatus(Milliseconds currentTime) {
#if ESP32_BLE
  return Esp32BleNetwork::get()->getStatusStr(currentTime);
#else   // ESP32_BLE
  return "Not supported";
#endif  // ESP32_BLE
}

std::string otherStatus(Player& player, Milliseconds currentTime) {
  char otherStatusStr[100] = "Leading";
  if (player.followedNextHopNetwork() == GetWiFiNetwork()) {
    snprintf(otherStatusStr, sizeof(otherStatusStr) - 1, "Following Wi-Fi nh=%u", player.currentNumHops());
  }
#if ESP32_BLE
  else if (player.followedNextHopNetwork() == Esp32BleNetwork::get()) {
    snprintf(otherStatusStr, sizeof(otherStatusStr) - 1, "Following BLE nh=%u", player.currentNumHops());
  }
#endif  // ESP32_BLE
  return std::string(otherStatusStr);
}

}  // namespace jazzlights

#endif  // WEARABLE
