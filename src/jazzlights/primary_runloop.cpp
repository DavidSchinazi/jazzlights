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
class PhoneButtonHandler : public GpioButton::Interface {
 public:
  PhoneButtonHandler() = default;
  ~PhoneButtonHandler() = default;
  void ShortPress(uint8_t pin, Milliseconds currentTime) override {}
  void LongPress(uint8_t pin, Milliseconds currentTime) override {}
  void HeldDown(uint8_t pin, Milliseconds currentTime) override {}
};
static PhoneButtonHandler* GetPhoneButtonHandler() {
  static PhoneButtonHandler buttonHandler;
  return &buttonHandler;
}
#if JL_IS_CONTROLLER(ATOM_MATRIX)
static constexpr uint8_t kPhonePin1 = 32;
static constexpr uint8_t kPhonePin2 = 26;
#elif JL_IS_CONTROLLER(ATOM_S3)
static constexpr uint8_t kPhonePin1 = 1;
static constexpr uint8_t kPhonePin2 = 2;
#else
#error "unsupported controller for phone"
#endif
static GpioButton* GetPhoneButton1() {
  static GpioButton button(kPhonePin1, *GetPhoneButtonHandler(), timeMillis());
  return &button;
}
static GpioButton* GetPhoneButton2() {
  static GpioButton button(kPhonePin2, *GetPhoneButtonHandler(), timeMillis());
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
  (void)GetPhoneButton1();
  (void)GetPhoneButton2();
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
  GetPhoneButton1()->RunLoop(currentTime);
  GetPhoneButton2()->RunLoop(currentTime);
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
