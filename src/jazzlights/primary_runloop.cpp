#include "jazzlights/primary_runloop.h"

#include "jazzlights/util/config.h"

#ifdef ESP32

#include <memory>
#include <mutex>

#include "jazzlights/layout/layout_data.h"
#include "jazzlights/motor/motor.h"
#include "jazzlights/network/esp32_ble.h"
#include "jazzlights/network/ethernet.h"
#include "jazzlights/network/manager.h"
#include "jazzlights/network/wifi.h"
#include "jazzlights/orrery/max485_bus.h"
#include "jazzlights/orrery/orrery_common.h"
#include "jazzlights/orrery/orrery_leader.h"
#include "jazzlights/orrery/orrery_planet.h"
#include "jazzlights/render/fastled_runner.h"
#include "jazzlights/render/player.h"
#include "jazzlights/ui/rotary_phone.h"
#include "jazzlights/ui/ui.h"
#include "jazzlights/ui/ui_atom_matrix.h"
#include "jazzlights/ui/ui_atom_s3.h"
#include "jazzlights/ui/ui_audio.h"
#include "jazzlights/ui/ui_core2.h"
#include "jazzlights/ui/ui_disabled.h"
#include "jazzlights/ui/ui_m5stick_c.h"
#include "jazzlights/ui/ui_motor.h"
#include "jazzlights/ui/ui_orrery_leader.h"
#include "jazzlights/util/instrumentation.h"
#include "jazzlights/util/log.h"
#include "jazzlights/websocket_server.h"

#ifndef JL_TEST_MOTOR
#define JL_TEST_MOTOR 0
#endif  // JL_TEST_MOTOR

namespace jazzlights {

NetworkManager gNetworkManager;
Player gPlayer(gNetworkManager);
FastLedRunner gRunner(&gPlayer);

#if JL_IS_CONTROLLER(ATOM_MATRIX)
typedef AtomMatrixUi Esp32UiImpl;
#elif JL_IS_CONTROLLER(ATOM_S3)
typedef AtomS3Ui Esp32UiImpl;
#elif JL_IS_CONTROLLER(M5STICK_C)
#if JL_AUDIO_VISUALIZER
typedef AudioVisualizerUi Esp32UiImpl;
#else   // JL_AUDIO_VISUALIZER
typedef M5StickCUi Esp32UiImpl;
#endif  // JL_AUDIO_VISUALIZER
#elif JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(CORES3)
#if JL_AUDIO_VISUALIZER
typedef AudioVisualizerUi Esp32UiImpl;
#elif JL_IS_CONFIG(ORRERY_LEADER)
typedef OrreryLeaderUi Esp32UiImpl;
#elif JL_MOTOR
typedef CoreMotorUi Esp32UiImpl;
#else   // JL_MOTOR
typedef Core2AwsUi Esp32UiImpl;
#endif  // JL_MOTOR
#else
typedef NoOpUi Esp32UiImpl;
#endif

static Esp32UiImpl* GetUi() {
  static Esp32UiImpl sUi(gPlayer);
  return &sUi;
}

#if JL_WEBSOCKET_SERVER
WebSocketServer gWebSocketServer(80, gPlayer);
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

  SetupLogging();

  GetUi()->SetFastLedRunner(&gRunner);
  GetUi()->InitialSetup();

  AddLedsToRunner(&gRunner);

#if JL_AUDIO_VISUALIZER
  // Ensures creatures follow the sound reactive dome.
  gPlayer.SetBasePrecedence(kCreatureOverridePrecedence);
  gPlayer.SetPrecedenceGain(0);
#elif JL_IS_CONFIG(CLOUDS)
#if !JL_DEV
  gPlayer.SetBasePrecedence(6000);
  gPlayer.SetPrecedenceGain(100);
#else   // JL_DEV
  gPlayer.SetBasePrecedence(5800);
  gPlayer.SetPrecedenceGain(100);
#endif  // JL_DEV
#elif JL_IS_CONFIG(WAND) || JL_IS_CONFIG(STAFF) || JL_IS_CONFIG(HAT) || JL_IS_CONFIG(SHOE) ||              \
    JL_IS_CONFIG(FAIRY_STRING) || JL_IS_CONFIG(NEW_HAT) || JL_IS_CONFIG(BOW) || JL_IS_CONFIG(RHINO_HAT) || \
    JL_IS_CONFIG(RHINO_STAFF)
  gPlayer.SetBasePrecedence(500);
  gPlayer.SetPrecedenceGain(100);
#elif JL_IS_CONFIG(XMAS_TREE)
  gPlayer.SetBasePrecedence(5000);
  gPlayer.SetPrecedenceGain(100);
#elif JL_IS_CONFIG(CREATURE)
  gPlayer.SetBasePrecedence(1);
  gPlayer.SetPrecedenceGain(0);
#elif JL_IS_CONFIG(ORRERY_LEADER)
  gPlayer.SetBasePrecedence(kOrreryLeaderBasePrecedence);
  gPlayer.SetPrecedenceGain(0);
#elif JL_IS_CONFIG(ORRERY_PLANET)
  gPlayer.SetBasePrecedence(kDefaultPlanetBasePrecedence);
  gPlayer.SetPrecedenceGain(kDefaultPlanetPrecedenceGain);
  gPlayer.SetBrightness(kDefaultPlanetBrightness);
  gPlayer.SetPlanetPattern(kPlanetPattern);
#elif JL_IS_CONFIG(GAUNTLET)
  gPlayer.SetBasePrecedence(7000);
  gPlayer.SetPrecedenceGain(1000);
#else
  gPlayer.SetBasePrecedence(1000);
  gPlayer.SetPrecedenceGain(1000);
#endif

  gNetworkManager.Connect(Esp32BleNetwork::Get());
#if JL_WIFI
  gNetworkManager.Connect(WiFiNetwork::Get());
#endif  // JL_WIFI
#if JL_ETHERNET
  gNetworkManager.Connect(EthernetNetwork::Get());
#endif  // JL_ETHERNET
#if JL_IS_CONFIG(ORRERY_PLANET) && !JL_ORRERY_PLUTO
  OrreryPlanet::Get()->Setup(gPlayer);
#elif JL_IS_CONFIG(ORRERY_LEADER)
  OrreryLeader::Get()->Setup(gPlayer);
#endif  // ORRERY
  gPlayer.Begin();

  GetUi()->FinalSetup();

  gRunner.Start();
#if JL_AUDIO_VISUALIZER
  Audio::Get().Setup();
#endif  // JL_AUDIO_VISUALIZER
}

void RunPrimaryRunLoop() {
  SAVE_TIME_POINT(PrimaryRunLoop, LoopStart);
  GetUi()->RunLoop();
#if JL_IS_CONFIG(ORRERY_LEADER)
  OrreryLeader::Get()->RunLoop();
#elif JL_IS_CONFIG(ORRERY_PLANET) && !JL_ORRERY_PLUTO
  OrreryPlanet::Get()->RunLoop();
#endif  // ORRERY
  SAVE_TIME_POINT(PrimaryRunLoop, UserInterface);
  Esp32BleNetwork::Get()->RunLoop();
  SAVE_TIME_POINT(PrimaryRunLoop, Bluetooth);

#if !JL_IS_CONFIG(PHONE)
  const bool shouldRender = gPlayer.Render();
#else   // PHONE
  PhonePinHandler::Get()->RunLoop();
  const bool shouldRender = true;
#endif  // !PHONE
  SAVE_TIME_POINT(PrimaryRunLoop, PlayerCompute);
  if (shouldRender) { gRunner.Render(); }
#if JL_WEBSOCKET_SERVER
  if (WiFiNetwork::Get()->Status() != kInitializing) {
    // This can't be called until after the networks have been initialized.
    gWebSocketServer.Start();
  }
#endif  // JL_WEBSOCKET_SERVER
  SAVE_TIME_POINT(PrimaryRunLoop, LoopEnd);
#if JL_TEST_MOTOR
  StepperMotorTestRunLoop();
#endif  // JL_TEST_MOTOR
}

}  // namespace jazzlights

#endif  // ESP32
