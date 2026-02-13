#include "jazzlights/network/uart.h"

#if JL_UART

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <cstring>

#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {

constexpr uart_port_t kUartNum = UART_NUM_2;

}  // namespace

void SetupUart(Milliseconds currentTime) {
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

void RunUart(Milliseconds currentTime) {
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

}  // namespace jazzlights

#endif  // JL_UART
