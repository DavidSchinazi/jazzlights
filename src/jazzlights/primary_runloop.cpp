#include "jazzlights/primary_runloop.h"

#include "jazzlights/config.h"

#ifdef ESP32

#include <memory>
#include <mutex>

#include "jazzlights/fastled_runner.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/layout_data.h"
#include "jazzlights/networks/arduino_ethernet.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/networks/wifi.h"
#include "jazzlights/player.h"
#include "jazzlights/ui/ui.h"
#include "jazzlights/ui/ui_atom_matrix.h"
#include "jazzlights/ui/ui_atom_s3.h"
#include "jazzlights/ui/ui_core2.h"
#include "jazzlights/ui/ui_disabled.h"
#include "jazzlights/websocket_server.h"

namespace jazzlights {

#if JL_ARDUINO_ETHERNET
ArduinoEthernetNetwork ethernetNetwork(WiFiNetwork::get()->getLocalDeviceId().PlusOne());
#endif  // JL_ARDUINO_ETHERNET
Player player;
FastLedRunner runner(&player);

#if JL_IS_CONTROLLER(ATOM_MATRIX)
typedef AtomMatrixUi Esp32UiImpl;
#elif JL_IS_CONTROLLER(ATOM_S3)
typedef AtomS3Ui Esp32UiImpl;
#elif JL_IS_CONTROLLER(CORE2AWS)
typedef Core2AwsUi Esp32UiImpl;
#else
typedef NoOpUi Esp32UiImpl;
#endif

static Esp32UiImpl* GetUi() {
  static Esp32UiImpl ui(player, timeMillis());
  return &ui;
}

#if JL_IS_CONFIG(PHONE)
#if JL_IS_CONTROLLER(ATOM_MATRIX)
static constexpr uint8_t kPhoneDigitPin = 32;
static constexpr uint8_t kPhoneDialPin = 26;
#elif JL_IS_CONTROLLER(ATOM_S3)
static constexpr uint8_t kPhoneDigitPin = 1;
static constexpr uint8_t kPhoneDialPin = 2;
#else
#error "unsupported controller for phone"
#endif
#define JL_PHONE_TIME_FMT "%03llds%03lldms%03lldus"
#define JL_PHONE_TIME_VAL(x) ((x) / 1000000), (((x) / 1000) % 1000), ((x) % 1000)

#define JL_PHONE_DEBUG_ENABLED 0

#if JL_PHONE_DEBUG_ENABLED
#define JL_PHONE_DEBUG(...) jll_info(__VA_ARGS__)
#else  // JL_PHONE_DEBUG_ENABLED
#define JL_PHONE_DEBUG(...) jll_debug(__VA_ARGS__)
#endif  // JL_PHONE_DEBUG_ENABLED

static constexpr int64_t kNumberTime = 3000000;  // 3s.

class PhonePinHandler : public GpioPin::PinInterface {
 public:
  PhonePinHandler() = default;
  ~PhonePinHandler() = default;
  void HandleChange(uint8_t pin, bool isClosed, int64_t timeOfChange) override {
    JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Pin %u %s", JL_PHONE_TIME_VAL(timeOfChange), pin,
                   (isClosed ? "closed" : "opened"));
    if (pin == kPhoneDialPin) {
      dialing_ = isClosed;
      if (dialing_) {
        JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Started dialing", JL_PHONE_TIME_VAL(timeOfChange));
      } else {
        JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Dialed %u lastKnownDigitIsClosed_=%s", JL_PHONE_TIME_VAL(timeOfChange),
                       digitCount_, (lastKnownDigitIsClosed_ ? "closed" : "open"));
        if (0 < digitCount_ && digitCount_ <= 10) {
          if (digitCount_ == 10) { digitCount_ = 0; }
          if (lastNumberEvent_ < 0 || timeOfChange - lastNumberEvent_ > kNumberTime) { fullNumber_ = 0; }
          fullNumber_ = fullNumber_ * 10 + digitCount_;
          jll_info(JL_PHONE_TIME_FMT " Dialed %llu", JL_PHONE_TIME_VAL(timeOfChange), fullNumber_);
          lastNumberEvent_ = timeOfChange;
        }
        digitCount_ = 0;
        lastDigitEvent_ = -1;
      }
    } else if (pin == kPhoneDigitPin) {
      if (lastKnownDigitIsClosed_ == isClosed) {
        JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " STRANGE unexpected repeat digit %s", JL_PHONE_TIME_VAL(timeOfChange),
                       (isClosed ? "open" : "closed"));
      } else {
        lastKnownDigitIsClosed_ = isClosed;
      }
      if (dialing_) {
        if (lastDigitEvent_ >= 0) {
          const int64_t timeListLastDigitEvent = timeOfChange - lastDigitEvent_;
          JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Digit pin was %s for " JL_PHONE_TIME_FMT, JL_PHONE_TIME_VAL(timeOfChange),
                         (isClosed ? "open" : "closed"), JL_PHONE_TIME_VAL(timeListLastDigitEvent));
          if (isClosed && timeListLastDigitEvent >= 50000 && timeListLastDigitEvent <= 120000) {
            JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Incrementing count from %u to %u", JL_PHONE_TIME_VAL(timeOfChange),
                           digitCount_, digitCount_ + 1);
            digitCount_++;
          }
          if (!isClosed && timeListLastDigitEvent >= 80000 && timeListLastDigitEvent <= 120000) {
            JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Maybe extra closed - incrementing count from %u to %u",
                           JL_PHONE_TIME_VAL(timeOfChange), digitCount_, digitCount_ + 1);
            digitCount_++;
          }
        } else {
          JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " %s digit pin", JL_PHONE_TIME_VAL(timeOfChange),
                         (isClosed ? "Closing" : "Opening"));
        }
        lastDigitEvent_ = timeOfChange;
      } else {
        const bool dialIsClosed = gpio_get_level(static_cast<gpio_num_t>(kPhoneDialPin)) == 0;
        JL_PHONE_DEBUG(JL_PHONE_TIME_FMT " Ignoring digit pin %s when not dialing - dial is %s",
                       JL_PHONE_TIME_VAL(timeOfChange), (isClosed ? "closed" : "opened"),
                       (dialIsClosed ? "UNEXPECTEDLY CLOSED" : "expectedly open"));
      }
    }
  }

 private:
  bool dialing_ = false;
  bool lastKnownDigitIsClosed_ = false;
  uint8_t digitCount_ = 0;
  int64_t lastDigitEvent_ = -1;
  uint64_t fullNumber_ = 0;
  int64_t lastNumberEvent_ = -1;
};
static PhonePinHandler* GetPhonePinHandler() {
  static PhonePinHandler pinHandler;
  return &pinHandler;
}
static GpioPin* GetPhonePin1() {
  static GpioPin button(kPhoneDigitPin, *GetPhonePinHandler());
  return &button;
}
static GpioPin* GetPhonePin2() {
  static GpioPin button(kPhoneDialPin, *GetPhonePinHandler());
  return &button;
}
#endif  // PHONE

#if JL_WEBSOCKET_SERVER
WebSocketServer websocket_server(80, player);
#endif  // JL_WEBSOCKET_SERVER

void SetupPrimaryRunLoop() {
#if JL_DEBUG
  // Sometimes the UART monitor takes a couple seconds to connect when JTAG is involved.
  // Sleep here to ensure we don't miss any logs.
  for (size_t i = 0; i < 20; i++) {
    if ((i % 10) == 0) { ets_printf("%02u ", i / 10); }
    ets_printf(".");
    if ((i % 10) == 9) { ets_printf("\n"); }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
#endif  // JL_DEBUG
  Milliseconds currentTime = timeMillis();
  GetUi()->set_fastled_runner(&runner);
  GetUi()->InitialSetup(currentTime);
#if JL_IS_CONFIG(PHONE)
  (void)GetPhonePin1();
  (void)GetPhonePin2();
#endif  // PHONE

  AddLedsToRunner(&runner);

#if JL_IS_CONFIG(CLOUDS)
#if !JL_DEV
  player.setBasePrecedence(6000);
  player.setPrecedenceGain(100);
#else   // JL_DEV
  player.setBasePrecedence(5800);
  player.setPrecedenceGain(100);
#endif  // JL_DEV
#elif JL_IS_CONFIG(WAND) || JL_IS_CONFIG(STAFF) || JL_IS_CONFIG(HAT) || JL_IS_CONFIG(SHOE)
  player.setBasePrecedence(500);
  player.setPrecedenceGain(100);
#else
  player.setBasePrecedence(1000);
  player.setPrecedenceGain(1000);
#endif

  player.connect(Esp32BleNetwork::get());
  player.connect(WiFiNetwork::get());
#if JL_ARDUINO_ETHERNET
  player.connect(&ethernetNetwork);
#endif  // JL_ARDUINO_ETHERNET
  player.begin(currentTime);

  GetUi()->FinalSetup(currentTime);

  runner.Start();
}

void RunPrimaryRunLoop() {
  SAVE_TIME_POINT(PrimaryRunLoop, LoopStart);
  Milliseconds currentTime = timeMillis();
  GetUi()->RunLoop(currentTime);
#if JL_IS_CONFIG(PHONE)
  GetPhonePin1()->RunLoop();
  GetPhonePin2()->RunLoop();
#endif  // PHONE
  SAVE_TIME_POINT(PrimaryRunLoop, UserInterface);
  Esp32BleNetwork::get()->runLoop(currentTime);
  SAVE_TIME_POINT(PrimaryRunLoop, Bluetooth);

#if !JL_IS_CONFIG(PHONE)
  const bool shouldRender = player.render(currentTime);
#else   // PHONE
  const bool shouldRender = true;
#endif  // !PHONE
  SAVE_TIME_POINT(PrimaryRunLoop, PlayerCompute);
  if (shouldRender) { runner.Render(); }
#if JL_WEBSOCKET_SERVER
  if (WiFiNetwork::get()->status() != INITIALIZING) {
    // This can't be called until after the networks have been initialized.
    websocket_server.Start();
  }
#endif  // JL_WEBSOCKET_SERVER
  SAVE_TIME_POINT(PrimaryRunLoop, LoopEnd);
}

}  // namespace jazzlights

#endif  // ESP32
