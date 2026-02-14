#include "jazzlights/network/uart.h"

#if JL_UART

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <cstring>
#include <mutex>

#include "jazzlights/esp32_shared.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {

constexpr size_t kUartBufferSize = 1024;
constexpr size_t kPrintBufferSize = 2 * kUartBufferSize;
constexpr uart_event_type_t kApplicationDataAvailable = static_cast<uart_event_type_t>(UART_EVENT_MAX + 1);
static_assert(kApplicationDataAvailable > UART_EVENT_MAX, "UART event types wrapped around");

class UartHandler {
 public:
  static UartHandler* Get();
  void WriteData(const uint8_t* buffer, size_t length);
  size_t ReadData(uint8_t* buffer, size_t maxLength);

 private:
  explicit UartHandler(uart_port_t uartPort, int txPin, int rxPin);
  ~UartHandler();
  static void TaskFunction(void* parameters);
  void SetUp();
  void RunTask();

  const uart_port_t uartPort_;         // Only modified in constructor.
  const int txPin_;                    // Only modified in constructor.
  const int rxPin_;                    // Only modified in constructor.
  TaskHandle_t taskHandle_ = nullptr;  // Only modified in constructor.
  std::mutex sendMutex_;
  std::mutex recvMutex_;
  uint8_t* sharedSendBuffer_ = nullptr;  // Protected by `sendMutex_`.
  size_t lengthInSharedSendBuffer_ = 0;  // Protected by `sendMutex_`.
  uint8_t* taskSendBuffer_ = nullptr;    // Only accessed by task.
  size_t lengthInTaskSendBuffer_ = 0;    // Only accessed by task.
  uint8_t* taskRecvBuffer_ = nullptr;    // Only accessed by task.
  uint8_t* sharedRecvBuffer_ = nullptr;  // Protected by `recvMutex_`.
  size_t lengthInSharedRecvBuffer_ = 0;  // Protected by `recvMutex_`.
  QueueHandle_t queue_;
};

UartHandler* UartHandler::Get() {
  constexpr uart_port_t kUartPort = UART_NUM_2;
  constexpr int kTxPin = 26;
  constexpr int kRxPin = 32;
  static UartHandler sHandler(kUartPort, kTxPin, kRxPin);
  return &sHandler;
}

UartHandler::UartHandler(uart_port_t uartPort, int txPin, int rxPin)
    : uartPort_(uartPort), txPin_(txPin), rxPin_(rxPin) {
  // Pin task to core 0 to ensure UART interrupts do not get in the way of LED writing on core 1.
  if (xTaskCreatePinnedToCore(TaskFunction, "JL_UART", configMINIMAL_STACK_SIZE + 2000,
                              /*parameters=*/this, kHighestTaskPriority, &taskHandle_,
                              /*coreID=*/0) != pdPASS) {
    jll_fatal("Failed to create UartHandler task");
  }
  sharedSendBuffer_ = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (sharedSendBuffer_ == nullptr) {
    jll_fatal("Failed to allocate UartHandler shared send buffer size %d", kUartBufferSize);
  }
  taskSendBuffer_ = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (taskSendBuffer_ == nullptr) {
    jll_fatal("Failed to allocate UartHandler task send buffer size %d", kUartBufferSize);
  }
  taskRecvBuffer_ = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (taskRecvBuffer_ == nullptr) { jll_fatal("Failed to allocate UartHandler recv buffer size %d", kUartBufferSize); }
  sharedRecvBuffer_ = reinterpret_cast<uint8_t*>(malloc(kPrintBufferSize));
  if (sharedRecvBuffer_ == nullptr) {
    jll_fatal("Failed to allocate UartHandler print buffer size %d", kPrintBufferSize);
  }
}

UartHandler::~UartHandler() {
  vTaskDelete(taskHandle_);
  free(sharedSendBuffer_);
  free(taskSendBuffer_);
  free(taskRecvBuffer_);
  free(sharedRecvBuffer_);
}

// static
void UartHandler::TaskFunction(void* parameters) {
  UartHandler* handler = static_cast<UartHandler*>(parameters);
  handler->SetUp();
  while (true) { handler->RunTask(); }
}

void UartHandler::RunTask() {
  uart_event_t event;
  if (!xQueueReceive(queue_, &event, portMAX_DELAY)) { jll_fatal("UartHandler queue error"); }
  switch (event.type) {
    case UART_DATA: {  // There is data ready to be read.
      size_t lengthToRead = std::min<size_t>(event.size, kUartBufferSize);
      if (lengthToRead > 0) {
        int readLen = uart_read_bytes(uartPort_, taskRecvBuffer_, lengthToRead, portMAX_DELAY);
        if (readLen <= 0) {
          jll_error("UART%d read error %d", uartPort_, readLen);
        } else {
          const std::lock_guard<std::mutex> lock(recvMutex_);
          size_t lengthToCopy = std::min<size_t>(readLen, kUartBufferSize - lengthInSharedRecvBuffer_);
          memcpy(sharedRecvBuffer_ + lengthInSharedRecvBuffer_, taskRecvBuffer_, lengthToCopy);
          lengthInSharedRecvBuffer_ += lengthToCopy;
        }
      }
    } break;
    case UART_BREAK: {  // Received a UART break signal.
      jll_info("UART%d received break signal", uartPort_);
    } break;
    case UART_BUFFER_FULL: {  // The hardware receive buffer is full.
      jll_error("UART%d read full", uartPort_);
    } break;
    case UART_FIFO_OVF: {  // The hardware receive buffer has overflowed.
      jll_error("UART%d read overflow", uartPort_);
      ESP_ERROR_CHECK(uart_flush_input(uartPort_));
    } break;
    case UART_FRAME_ERR: {  // Received byte with data frame error.
      jll_error("UART%d data frame error", uartPort_);
    } break;
    case UART_PARITY_ERR: {  // Received byte with parity error.
      jll_error("UART%d parity error", uartPort_);
    } break;
    case UART_DATA_BREAK: {  // Sent a UART break signal.
      jll_info("UART%d sent break signal", uartPort_);
    } break;
    case UART_PATTERN_DET: {  // Detected pattern in received data.
      jll_error("UART%d pattern detected", uartPort_);
    } break;
    case kApplicationDataAvailable: {  // Our application has data to write to UART.
      {
        const std::lock_guard<std::mutex> lock(sendMutex_);
        lengthInTaskSendBuffer_ = lengthInSharedSendBuffer_;
        lengthInSharedSendBuffer_ = 0;
        memcpy(taskSendBuffer_, sharedSendBuffer_, lengthInTaskSendBuffer_);
      }
      int bytes_written =
          uart_write_bytes(uartPort_, reinterpret_cast<const char*>(taskSendBuffer_), lengthInTaskSendBuffer_);
      jll_info("UART%d wrote %d bytes", uartPort_, bytes_written);
    } break;
    default: {
      jll_info("UART%d unexpected event %d", uartPort_, static_cast<int>(event.type));
    } break;
  }
}

void UartHandler::SetUp() {
  constexpr int kEventQueueSize = 16;
  constexpr int kBaudRate = 9600;
  ESP_ERROR_CHECK(uart_driver_install(uartPort_, 2 * kUartBufferSize, 2 * kUartBufferSize, kEventQueueSize, &queue_,
                                      /*intr_alloc_flags=*/0));
  uart_config_t uart_config = {
      .baud_rate = kBaudRate,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,
      .source_clk = UART_SCLK_DEFAULT,
  };
  ESP_ERROR_CHECK(uart_param_config(uartPort_, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(uartPort_, txPin_, rxPin_, /*rts=*/UART_PIN_NO_CHANGE, /*cts=*/UART_PIN_NO_CHANGE));
}

void UartHandler::WriteData(const uint8_t* buffer, size_t length) {
  if (length > kUartBufferSize) {
    jll_error("UART%d refusing to send %zu bytes > %zu", uartPort_, length, kUartBufferSize);
    return;
  }
  {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    if (lengthInSharedSendBuffer_ + length > kUartBufferSize) {
      jll_error("UART%d cannot currently send %zu bytes, already have %zu and limit is %zu", uartPort_, length,
                lengthInSharedSendBuffer_, kUartBufferSize);
      return;
    }
    memcpy(sharedSendBuffer_ + lengthInSharedSendBuffer_, buffer, length);
    lengthInSharedSendBuffer_ += length;
  }
  uart_event_t eventToSend = {};
  eventToSend.type = kApplicationDataAvailable;
  BaseType_t res = xQueueSendToBack(queue_, &eventToSend, /*ticksToWait=*/0);
  if (res == errQUEUE_FULL) {
    jll_error("UART%d event queue is full", uartPort_);
  } else if (res != pdPASS) {
    jll_error("UART%d event queue error %d", uartPort_, res);
  }
}

size_t UartHandler::ReadData(uint8_t* buffer, size_t maxLength) {
  const std::lock_guard<std::mutex> lock(recvMutex_);
  const size_t readLength = std::min<size_t>(lengthInSharedRecvBuffer_, maxLength);
  memcpy(buffer, sharedRecvBuffer_, readLength);
  lengthInSharedRecvBuffer_ -= readLength;
  return readLength;
}

}  // namespace

void SetupUart(Milliseconds currentTime) { (void)UartHandler::Get(); }

void RunUart(Milliseconds currentTime) {
  static uint8_t* outerRecvBuffer = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (outerRecvBuffer == nullptr) {
    jll_fatal("Failed to allocate UartHandler outer recv buffer size %d", kUartBufferSize);
  }
  static uint8_t* printBuffer = reinterpret_cast<uint8_t*>(malloc(kPrintBufferSize));
  if (printBuffer == nullptr) { jll_fatal("Failed to allocate UartHandler print buffer size %d", kPrintBufferSize); }
  static Milliseconds sTimeBetweenRuns = UnpredictableRandom::GetNumberBetween(500, 1500);
  static Milliseconds sLastWriteTime = 0;
  size_t readLength = UartHandler::Get()->ReadData(outerRecvBuffer, kUartBufferSize);
  if (readLength > 0) {
    EscapeRawBuffer(outerRecvBuffer, readLength, printBuffer, kPrintBufferSize);
    jll_info("UART read %d bytes: %s", readLength, printBuffer);
  }

  if (currentTime - sLastWriteTime < sTimeBetweenRuns) {
    // Too soon to write.
    return;
  }
  sLastWriteTime = currentTime;
  sTimeBetweenRuns = UnpredictableRandom::GetNumberBetween(500, 1500);
  char send_data[100] = {};
  snprintf(send_data, sizeof(send_data) - 1, "\n\t\t\t\t\t\t" STRINGIFY(JL_ROLE) " is NEW sending: %u.", currentTime);
  UartHandler::Get()->WriteData(reinterpret_cast<const uint8_t*>(send_data), strnlen(send_data, sizeof(send_data)));
}

}  // namespace jazzlights

#endif  // JL_UART
