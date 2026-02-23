#include "jazzlights/primary_runloop.h"

#include "jazzlights/config.h"

#ifdef ESP32

#include <memory>
#include <mutex>

#include "jazzlights/fastled_runner.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/layout/layout_data.h"
#include "jazzlights/motor.h"
#include "jazzlights/network/esp32_ble.h"
#include "jazzlights/network/ethernet.h"
#include "jazzlights/network/max485_bus.h"
#include "jazzlights/network/wifi.h"
#include "jazzlights/player.h"
#include "jazzlights/ui/hall_sensor.h"
#include "jazzlights/ui/rotary_phone.h"
#include "jazzlights/ui/ui.h"
#include "jazzlights/ui/ui_atom_matrix.h"
#include "jazzlights/ui/ui_atom_s3.h"
#include "jazzlights/ui/ui_core2.h"
#include "jazzlights/ui/ui_disabled.h"
#include "jazzlights/util/log.h"
#include "jazzlights/websocket_server.h"

namespace jazzlights {

Player player;
FastLedRunner runner(&player);

#if JL_IS_CONTROLLER(ATOM_MATRIX)
typedef AtomMatrixUi Esp32UiImpl;
#elif JL_IS_CONTROLLER(ATOM_S3)
typedef AtomS3Ui Esp32UiImpl;
#elif JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(CORES3)
typedef Core2AwsUi Esp32UiImpl;
#else
typedef NoOpUi Esp32UiImpl;
#endif

static Esp32UiImpl* GetUi() {
  static Esp32UiImpl ui(player, timeMillis());
  return &ui;
}

#if JL_WEBSOCKET_SERVER
WebSocketServer websocket_server(80, player);
#endif  // JL_WEBSOCKET_SERVER

#if JL_HALL_SENSOR
HallSensor* GetHallSensor() {
  constexpr uint8_t kHallSensorPin = 33;
  static HallSensor sHallSensor(kHallSensorPin);
  return &sHallSensor;
}
#endif  // JL_HALL_SENSOR

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
  SetupMax485Bus(currentTime);

  AddLedsToRunner(&runner);

#if JL_IS_CONFIG(CLOUDS)
#if !JL_DEV
  player.setBasePrecedence(6000);
  player.setPrecedenceGain(100);
#else   // JL_DEV
  player.setBasePrecedence(5800);
  player.setPrecedenceGain(100);
#endif  // JL_DEV
#elif JL_IS_CONFIG(WAND) || JL_IS_CONFIG(STAFF) || JL_IS_CONFIG(HAT) || JL_IS_CONFIG(SHOE) || JL_IS_CONFIG(FAIRY_STRING)
  player.setBasePrecedence(500);
  player.setPrecedenceGain(100);
#elif JL_IS_CONFIG(XMAS_TREE)
  player.setBasePrecedence(5000);
  player.setPrecedenceGain(100);
#elif JL_IS_CONFIG(CREATURE)
  player.setBasePrecedence(1);
  player.setPrecedenceGain(0);
#else
  player.setBasePrecedence(1000);
  player.setPrecedenceGain(1000);
#endif

  player.connect(Esp32BleNetwork::get());
#if JL_WIFI
  player.connect(WiFiNetwork::get());
#endif  // JL_WIFI
#if JL_ETHERNET
  player.connect(EthernetNetwork::get());
#endif  // JL_ETHERNET
  player.begin(currentTime);

  GetUi()->FinalSetup(currentTime);

  runner.Start();
#if JL_HALL_SENSOR
  (void)GetHallSensor();
#endif  // JL_HALL_SENSOR
}

void RunPrimaryRunLoop() {
  SAVE_TIME_POINT(PrimaryRunLoop, LoopStart);
  Milliseconds currentTime = timeMillis();
  GetUi()->RunLoop(currentTime);
  RunMax485Bus(currentTime);
  SAVE_TIME_POINT(PrimaryRunLoop, UserInterface);
  Esp32BleNetwork::get()->runLoop(currentTime);
  SAVE_TIME_POINT(PrimaryRunLoop, Bluetooth);

#if !JL_IS_CONFIG(PHONE)
  const bool shouldRender = player.render(currentTime);
#else   // PHONE
  PhonePinHandler::Get()->RunLoop();
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
#if JL_HALL_SENSOR
  GetHallSensor()->RunLoop();
#endif  // JL_HALL_SENSOR
#if JL_MOTOR
  StepperMotorTestRunLoop(currentTime);
#endif  // JL_MOTOR
}

}  // namespace jazzlights

#endif  // ESP32
