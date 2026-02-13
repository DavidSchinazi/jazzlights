#include "jazzlights/primary_runloop.h"

#include "jazzlights/config.h"

#ifdef ESP32

#if JL_IS_CONFIG(ORRERY)
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif  // ORRERY

#include <memory>
#include <mutex>

#include "jazzlights/fastled_runner.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/layout/layout_data.h"
#include "jazzlights/network/esp32_ble.h"
#include "jazzlights/network/ethernet.h"
#include "jazzlights/network/wifi.h"
#include "jazzlights/player.h"
#include "jazzlights/ui/rotary_phone.h"
#include "jazzlights/ui/ui.h"
#include "jazzlights/ui/ui_atom_matrix.h"
#include "jazzlights/ui/ui_atom_s3.h"
#include "jazzlights/ui/ui_core2.h"
#include "jazzlights/ui/ui_disabled.h"
#include "jazzlights/util/log.h"
#include "jazzlights/websocket_server.h"

namespace jazzlights {

#if JL_IS_CONFIG(ORRERY)
static constexpr uart_port_t kUartNum = UART_NUM_2;

void SetupUart() {
  constexpr int kUartBufferSize = 2048;
  constexpr int kTxPin = 26;
  constexpr int kRxPin = 32;
  QueueHandle_t uart_queue;
  ESP_ERROR_CHECK(uart_driver_install(kUartNum, kUartBufferSize, kUartBufferSize, 10, &uart_queue, 0));
  uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_APB,
  };
  ESP_ERROR_CHECK(uart_param_config(kUartNum, &uart_config));
  ESP_ERROR_CHECK(
      uart_set_pin(kUartNum, /*tx=*/kTxPin, /*rx=*/kRxPin, /*rts=*/UART_PIN_NO_CHANGE, /*cts=*/UART_PIN_NO_CHANGE));
}

void RunUart() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  char send_data[100] = {};
  snprintf(send_data, sizeof(send_data) - 1, "\n" STRINGIFY(JL_ROLE) " is sending: %u.", timeMillis());

  int bytes_written = uart_write_bytes_with_break(kUartNum, (const char*)send_data, strlen(send_data), 100);
  jll_error("Wrote %d bytes to UART", bytes_written);
  vTaskDelay(10 / portTICK_PERIOD_MS);

  uint8_t data[500];
  size_t length_to_read;
  ESP_ERROR_CHECK(uart_get_buffered_data_len(kUartNum, &length_to_read));
  if (length_to_read != 0) { jll_error("Buffered read %zu bytes from UART", length_to_read); }
  int bytes_read = uart_read_bytes(kUartNum, data, static_cast<uint32_t>(sizeof(data)), 100);
  if (bytes_read != 0) {
    uint8_t data2[700];
    EscapeRawBuffer(data, bytes_read, data2, sizeof(data2));
    jll_error("Read %d bytes from UART: %s", bytes_read, data2);
  }
}

#endif  // ORRERY

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

#if JL_IS_CONFIG(ORRERY)
  SetupUart();
#endif  // ORRERY
}

void RunPrimaryRunLoop() {
  SAVE_TIME_POINT(PrimaryRunLoop, LoopStart);
  Milliseconds currentTime = timeMillis();
  GetUi()->RunLoop(currentTime);
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

#if JL_IS_CONFIG(ORRERY)
  RunUart();
#endif
}

}  // namespace jazzlights

#endif  // ESP32
