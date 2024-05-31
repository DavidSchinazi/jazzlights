#include "jazzlights/arduino_loop.h"

#include "jazzlights/config.h"

#ifdef ARDUINO

#include <memory>
#include <mutex>

#include "jazzlights/fastled_runner.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/layout_data.h"
#include "jazzlights/networks/arduino_esp_wifi.h"
#include "jazzlights/networks/arduino_ethernet.h"
#include "jazzlights/networks/esp32_ble.h"
#include "jazzlights/player.h"
#include "jazzlights/rest_server.h"
#include "jazzlights/ui/ui.h"
#include "jazzlights/ui/ui_atom_matrix.h"
#include "jazzlights/ui/ui_atom_s3.h"
#include "jazzlights/ui/ui_core2.h"
#include "jazzlights/ui/ui_disabled.h"

namespace jazzlights {

#if JL_ARDUINO_ETHERNET
ArduinoEthernetNetwork ethernetNetwork(ArduinoEspWiFiNetwork::get()->getLocalDeviceId().PlusOne());
#endif  // JL_ARDUINO_ETHERNET
Player player;
FastLedRunner runner(&player);

#if JL_IS_CONTROLLER(ATOM_MATRIX)
typedef AtomMatrixUi ArduinoUiImpl;
#elif JL_IS_CONTROLLER(ATOM_S3)
typedef AtomS3Ui ArduinoUiImpl;
#elif JL_IS_CONTROLLER(CORE2AWS)
typedef Core2AwsUi ArduinoUiImpl;
#else
typedef NoOpUi ArduinoUiImpl;
#endif

ArduinoUiImpl ui(player, timeMillis());

#if JL_REST_SERVER
RestServer rest_server(80, player);
#endif  // JL_REST_SERVER

void arduinoSetup(void) {
  Milliseconds currentTime = timeMillis();
  Serial.begin(115200);
  ui.InitialSetup(currentTime);

  AddLedsToRunner(&runner);

#if JL_IS_CONFIG(WAND) || JL_IS_CONFIG(STAFF) || JL_IS_CONFIG(CAPTAIN_HAT) || JL_IS_CONFIG(SHOE)
  player.setBasePrecedence(500);
  player.setPrecedenceGain(100);
#else
  player.setBasePrecedence(1000);
  player.setPrecedenceGain(1000);
#endif

  player.connect(Esp32BleNetwork::get());
  player.connect(ArduinoEspWiFiNetwork::get());
#if JL_ARDUINO_ETHERNET
  player.connect(&ethernetNetwork);
#endif  // JL_ARDUINO_ETHERNET
  player.begin(currentTime);

  ui.FinalSetup(currentTime);

  runner.Start();
}

void arduinoLoop(void) {
  SAVE_TIME_POINT(LoopStart);
  Milliseconds currentTime = timeMillis();
  ui.RunLoop(currentTime);
  SAVE_TIME_POINT(UserInterface);
  Esp32BleNetwork::get()->runLoop(currentTime);
  SAVE_TIME_POINT(Bluetooth);

  const bool shouldRender = player.render(currentTime);
  SAVE_TIME_POINT(Player);
  if (shouldRender) { runner.Render(); }
#if JL_REST_SERVER
  if (ArduinoEspWiFiNetwork::get()->status() != INITIALIZING) {
    // This can't be called until after the networks have been initialized.
    rest_server.Start();
  }
#endif  // JL_REST_SERVER
}

}  // namespace jazzlights

#endif  // ARDUINO
