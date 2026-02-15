#include "jazzlights/network/uart.h"

#if JL_UART

#include <driver/uart.h>
#include <esp_crc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <cstring>
#include <mutex>

#include "jazzlights/esp32_shared.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/cobs.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {

#ifndef JL_LOG_UART_DATA
#define JL_LOG_UART_DATA 0
#endif  // JL_LOG_UART_DATA

#if JL_LOG_UART_DATA
#define jll_uart_data(...) jll_info(__VA_ARGS__)
#define jll_uart_data_buffer(...) jll_buffer_info(__VA_ARGS__)
#else  // JL_LOG_UART_DATA
#define jll_uart_data(...) jll_debug(__VA_ARGS__)
#define jll_uart_data_buffer(...) jll_buffer_debug(__VA_ARGS__)
#endif  // JL_LOG_UART_DATA

using BusId = uint8_t;

constexpr uint8_t kSeparator = 0;
constexpr BusId kBusIdEndOfMessage = 1;
constexpr BusId kBusIdBroadcast = 2;
constexpr BusId kBusIdLeader = 3;

#ifndef JL_BUS_ID
#if JL_BUS_LEADER
#define JL_BUS_ID kBusIdLeader
#else  // JL_BUS_ID
#error "Need to define bus ID for non-leaders"
#endif  // JL_BUS_ID
#endif  // JL_BUS_LEADER

// While most bus ID values are available, we're reserving values above 128 for future use.
static_assert((JL_BUS_ID) < 128, "high bus ID");
static_assert((JL_BUS_ID) >= kBusIdLeader, "low bus ID");
#if !JL_BUS_LEADER
static_assert((JL_BUS_ID) != kBusIdLeader, "follower bus ID cannot be equal to leader");
#endif  // JL_BUS_LEADER

constexpr BusId kBusId = (JL_BUS_ID);

constexpr size_t kUartBufferSize = 1024;
constexpr uart_event_type_t kApplicationDataAvailable = static_cast<uart_event_type_t>(UART_EVENT_MAX + 1);
static_assert(kApplicationDataAvailable > UART_EVENT_MAX, "UART event types wrapped around");

class UartHandler {
 public:
  explicit UartHandler(uart_port_t uartPort, int txPin, int rxPin);
  ~UartHandler();
  void WriteData(const uint8_t* buffer, size_t length);
  size_t ReadData(uint8_t* buffer, size_t maxLength);

 private:
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
  if (taskRecvBuffer_ == nullptr) {
    jll_fatal("Failed to allocate UartHandler task recv buffer size %d", kUartBufferSize);
  }
  sharedRecvBuffer_ = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (sharedRecvBuffer_ == nullptr) {
    jll_fatal("Failed to allocate UartHandler shared recv buffer size %d", kUartBufferSize);
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
      jll_uart_data("UART%d received break signal", uartPort_);
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
        size_t lengthToCopy = std::min<size_t>(lengthInSharedSendBuffer_, kUartBufferSize - lengthInTaskSendBuffer_);
        memcpy(taskSendBuffer_ + lengthInTaskSendBuffer_, sharedSendBuffer_, lengthToCopy);
        if (lengthToCopy < lengthInSharedSendBuffer_) {
          memmove(sharedSendBuffer_, sharedSendBuffer_ + lengthToCopy, lengthInSharedSendBuffer_ - lengthToCopy);
        }
        lengthInTaskSendBuffer_ += lengthToCopy;
        lengthInSharedSendBuffer_ -= lengthToCopy;
      }
      int bytes_written =
          uart_write_bytes(uartPort_, reinterpret_cast<const char*>(taskSendBuffer_), lengthInTaskSendBuffer_);
      if (bytes_written > 0) {
        if (static_cast<size_t>(bytes_written) == lengthInTaskSendBuffer_) {
          jll_uart_data("UART%d fully wrote %d bytes", uartPort_, bytes_written);
          lengthInTaskSendBuffer_ = 0;
        } else {
          jll_uart_data("UART%d partially wrote %d/%zu bytes", uartPort_, bytes_written, lengthInTaskSendBuffer_);
          lengthInTaskSendBuffer_ -= bytes_written;
          memmove(taskSendBuffer_, taskSendBuffer_ + bytes_written, lengthInTaskSendBuffer_ - bytes_written);
        }
      } else {
        jll_error("UART%d failed to write %zu bytes: got %d", uartPort_, lengthInTaskSendBuffer_, bytes_written);
      }

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

class Rs485Bus {
 public:
  void WriteMessage(const uint8_t* buffer, size_t length, BusId destBusId);
  size_t ReadMessage(uint8_t* buffer, size_t maxLength, BusId* outDestBusId);
  static Rs485Bus* Get();

  constexpr size_t ComputeExpansion(size_t length) {
    return /*kSeparator=*/1 + /*busID=*/1 + CobsMaxEncodedSize(length) + /*CRC32=*/sizeof(uint32_t) + /*kSeparator=*/1 +
           /*endOfMessage=*/1;
  }

 private:
  explicit Rs485Bus(BusId bus_id, uart_port_t uartPort, int txPin, int rxPin);
  size_t DecodeMessage(const uint8_t* encodedBuffer, size_t encodedLength, uint8_t* decodedBuffer,
                       size_t maxDecodedLength);
  void ShiftEncodedReadBuffer(size_t messageStartIndex);
  BusId bus_id_;
  UartHandler uart_;
  uint8_t* encodedReadBuffer_ = nullptr;
  size_t encodedReadBufferIndex_ = 0;
  uint8_t* decodedReadBuffer_ = nullptr;
  size_t decodedReadBufferIndex_ = 0;
};

Rs485Bus::Rs485Bus(BusId bus_id, uart_port_t uartPort, int txPin, int rxPin)
    : bus_id_(bus_id), uart_(uartPort, txPin, rxPin) {
  encodedReadBuffer_ = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (encodedReadBuffer_ == nullptr) {
    jll_fatal("Failed to allocate Rs485Bus encoded read buffer size %d", kUartBufferSize);
  }
  decodedReadBuffer_ = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (decodedReadBuffer_ == nullptr) {
    jll_fatal("Failed to allocate Rs485Bus decoded read buffer size %d", kUartBufferSize);
  }
}

// static
Rs485Bus* Rs485Bus::Get() {
  constexpr uart_port_t kUartPort = UART_NUM_2;
  constexpr int kTxPin = 26;
  constexpr int kRxPin = 32;
  static Rs485Bus sRs485Bus(kBusId, kUartPort, kTxPin, kRxPin);
  return &sRs485Bus;
}

void Rs485Bus::WriteMessage(const uint8_t* buffer, size_t length, BusId destBusId) {
  if (ComputeExpansion(length) > kUartBufferSize) {
    jll_error("Cannot write message of length %zu", length);
    return;
  }
  uint8_t uartBuffer1[kUartBufferSize];
  uartBuffer1[0] = kSeparator;
  uartBuffer1[1] = destBusId;
  memcpy(uartBuffer1 + 2, buffer, length);
  size_t uartBufferIndex1 = 2 + length;
  jll_uart_data_buffer(uartBuffer1, uartBufferIndex1, "computing outgoing CRC32");
  uint32_t crc32 = esp_crc32_be(0, uartBuffer1, uartBufferIndex1);
  memcpy(uartBuffer1 + uartBufferIndex1, &crc32, sizeof(crc32));
  uartBufferIndex1 += sizeof(crc32);
  // TODO change the COBS API so we can do this without a second buffer copy.
  uint8_t uartBuffer2[kUartBufferSize];
  uartBuffer2[0] = kSeparator;
  uartBuffer2[1] = destBusId;
  size_t uartBufferIndex2 =
      2 + CobsEncode(uartBuffer1 + 2, uartBufferIndex1 - 2, uartBuffer2 + 2, sizeof(uartBuffer2) - 2);
  uartBuffer2[uartBufferIndex2++] = kSeparator;
  uartBuffer2[uartBufferIndex2++] = kBusIdEndOfMessage;
  jll_uart_data_buffer(uartBuffer2, uartBufferIndex2, "Rs485Bus writing");
  return uart_.WriteData(uartBuffer2, uartBufferIndex2);
}

size_t Rs485Bus::DecodeMessage(const uint8_t* encodedBuffer, size_t encodedLength, uint8_t* decodedBuffer,
                               size_t maxDecodedLength) {
  jll_uart_data_buffer(encodedBuffer, encodedLength, "DecodeMessage attempting to parse");
  if (encodedLength < ComputeExpansion(0)) {
    jll_buffer_error(encodedBuffer, encodedLength, "Rs485Bus DecodeMessage ignoring very short message");
    return 0;
  }
  size_t uartBufferIndex = 0;
  BusId separator = encodedBuffer[uartBufferIndex++];
  if (separator != kSeparator) {
    jll_buffer_error(encodedBuffer, encodedLength,
                     "Rs485Bus DecodeMessage ignoring message due to bad initial separator");
    return 0;
  }
  BusId destBusId = encodedBuffer[uartBufferIndex++];
  if (destBusId != kBusIdBroadcast && destBusId != kBusId) {
    jll_buffer_error(encodedBuffer, encodedLength, "Rs485Bus DecodeMessage ignoring message not meant for us");
    return 0;
  }
  if (encodedBuffer[encodedLength - 2] != kSeparator) {
    jll_buffer_error(encodedBuffer, encodedLength, "Rs485Bus DecodeMessage did not find end separator");
    return 0;
  }
  if (encodedBuffer[encodedLength - 1] != kBusIdEndOfMessage) {
    jll_buffer_error(encodedBuffer, encodedLength, "Rs485Bus DecodeMessage did not find end of message");
    return 0;
  }
  decodedReadBufferIndex_ = 0;
  decodedReadBuffer_[decodedReadBufferIndex_++] = encodedBuffer[0];
  decodedReadBuffer_[decodedReadBufferIndex_++] = encodedBuffer[1];
  size_t decodeLength =
      CobsDecode(encodedBuffer + uartBufferIndex, encodedLength - uartBufferIndex - 2,
                 decodedReadBuffer_ + decodedReadBufferIndex_, kUartBufferSize - decodedReadBufferIndex_);
  if (decodeLength == 0) {
    jll_buffer_error(encodedBuffer, encodedLength, "Rs485Bus DecodeMessage failed to CobsDecode incoming message");
    return 0;
  }
  decodedReadBufferIndex_ += decodeLength;
  jll_uart_data_buffer(decodedReadBuffer_, decodedReadBufferIndex_ - sizeof(uint32_t),
                       "Rs485Bus DecodeMessage computing incoming CRC32");
  uint32_t crc32 = esp_crc32_be(0, decodedReadBuffer_, decodedReadBufferIndex_ - sizeof(uint32_t));
  if (memcmp(decodedReadBuffer_ + decodedReadBufferIndex_ - sizeof(crc32), &crc32, sizeof(uint32_t)) != 0) {
    jll_buffer_error(decodedReadBuffer_, decodedReadBufferIndex_,
                     "Rs485Bus DecodeMessage CRC32 check on %d bytes failed",
                     decodedReadBufferIndex_ - sizeof(uint32_t));
    return 0;
  }
  size_t decodedMessageLength = decodedReadBufferIndex_ - 2 - sizeof(uint32_t);
  if (decodedMessageLength > maxDecodedLength) {
    jll_error("Rs485Bus DecodeMessage found message too long %zu > %zu", decodedMessageLength, maxDecodedLength);
    return 0;
  }
  memcpy(decodedBuffer, decodedReadBuffer_ + 2, decodedMessageLength);
  return decodedMessageLength;
}

void Rs485Bus::ShiftEncodedReadBuffer(size_t messageStartIndex) {
  if (messageStartIndex == 0) { return; }
  if (encodedReadBufferIndex_ <= messageStartIndex) {
    encodedReadBufferIndex_ = 0;
    return;
  }
  // Move the message left to the start of the buffer to leave more room for future reads.
  memmove(encodedReadBuffer_, encodedReadBuffer_ + messageStartIndex, encodedReadBufferIndex_ - messageStartIndex);
  encodedReadBufferIndex_ -= messageStartIndex;
}

size_t Rs485Bus::ReadMessage(uint8_t* buffer, size_t maxLength, BusId* outDestBusId) {
  size_t readLength =
      uart_.ReadData(encodedReadBuffer_ + encodedReadBufferIndex_, kUartBufferSize - encodedReadBufferIndex_);
  if (readLength == 0) { return 0; }
  jll_uart_data_buffer(encodedReadBuffer_ + encodedReadBufferIndex_, readLength, "Rs485Bus read");
  encodedReadBufferIndex_ += readLength;
  jll_uart_data_buffer(encodedReadBuffer_, encodedReadBufferIndex_, "Rs485Bus data we now have");
  size_t messageStartIndex = 0;
  while (messageStartIndex < encodedReadBufferIndex_) {
    while (messageStartIndex < encodedReadBufferIndex_ && encodedReadBuffer_[messageStartIndex] != kSeparator) {
      messageStartIndex++;
    }
    if (messageStartIndex >= encodedReadBufferIndex_) {
      jll_uart_data_buffer(encodedReadBuffer_, encodedReadBufferIndex_,
                           "Rs485Bus discarding full read buffer without separator");
      encodedReadBufferIndex_ = 0;
      return 0;
    }
    // Now encodedReadBuffer_[messageStartIndex] is the separator.
    if (encodedReadBufferIndex_ - messageStartIndex < ComputeExpansion(0)) {
      jll_uart_data_buffer(encodedReadBuffer_, encodedReadBufferIndex_,
                           "Rs485Bus pausing because packet too short start=%zu", messageStartIndex);
      ShiftEncodedReadBuffer(messageStartIndex);
      return 0;
    }
    BusId destBusId = encodedReadBuffer_[messageStartIndex + 1];
    if (destBusId != kBusIdBroadcast && destBusId != kBusId) {
      messageStartIndex++;
      jll_uart_data_buffer(encodedReadBuffer_, encodedReadBufferIndex_,
                           "Rs485Bus skipping past message not meant for us, set start to %zu", messageStartIndex);
      continue;
    }
    // Now we've confirmed that the first two bytes at messageStartIndex indicate a message for us, now to find the end.
    size_t messageEndIndex = messageStartIndex + 2;
    while (messageEndIndex < encodedReadBufferIndex_ && encodedReadBuffer_[messageEndIndex] != kSeparator) {
      messageEndIndex++;
    }
    if (messageEndIndex + 1 >= encodedReadBufferIndex_) {
      jll_uart_data_buffer(encodedReadBuffer_, encodedReadBufferIndex_, "Rs485Bus pausing because packet incomplete");
      ShiftEncodedReadBuffer(messageStartIndex);
      return 0;
    }
    messageEndIndex++;
    if (encodedReadBuffer_[messageEndIndex] != kBusIdEndOfMessage) {
      jll_uart_data_buffer(encodedReadBuffer_, encodedReadBufferIndex_,
                           "Rs485Bus skipping past message without valid end");
      messageStartIndex = messageEndIndex;
      continue;
    }
    messageEndIndex++;
    size_t decodedMessageLength =
        DecodeMessage(encodedReadBuffer_ + messageStartIndex, messageEndIndex - messageStartIndex, buffer, maxLength);
    if (decodedMessageLength == 0) {
      jll_uart_data_buffer(encodedReadBuffer_, encodedReadBufferIndex_,
                           "Rs485Bus skipping past message which failed to decode start=%zu end=%zu", messageStartIndex,
                           messageEndIndex);
      messageStartIndex = messageEndIndex;
      continue;
    }
    ShiftEncodedReadBuffer(messageEndIndex);
    *outDestBusId = destBusId;
    return decodedMessageLength;
  }
  ShiftEncodedReadBuffer(messageStartIndex);
  return 0;
}

}  // namespace

void SetupUart(Milliseconds currentTime) { (void)Rs485Bus::Get(); }

void RunUart(Milliseconds currentTime) {
  static uint8_t* outerRecvBuffer = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (outerRecvBuffer == nullptr) {
    jll_fatal("Failed to allocate UartHandler outer recv buffer size %d", kUartBufferSize);
  }
  static uint8_t* outerSendBuffer = reinterpret_cast<uint8_t*>(malloc(kUartBufferSize));
  if (outerSendBuffer == nullptr) {
    jll_fatal("Failed to allocate UartHandler outer send buffer size %d", kUartBufferSize);
  }
  BusId destBusId = kSeparator;
  size_t readLength = Rs485Bus::Get()->ReadMessage(outerRecvBuffer, kUartBufferSize, &destBusId);
  if (readLength > 0) { jll_buffer_info(outerRecvBuffer, readLength, "UART read message"); }
#if !JL_BUS_LEADER
  if (destBusId == kBusId) {
    outerRecvBuffer[readLength] = '\0';
    int bytesWritten = snprintf(reinterpret_cast<char*>(outerSendBuffer), kUartBufferSize - 1,
                                "{" STRINGIFY(JL_ROLE) " is responding at %u to: %s}", currentTime, outerRecvBuffer);
    if (bytesWritten >= kUartBufferSize) { bytesWritten = kUartBufferSize - 1; }
    outerSendBuffer[bytesWritten] = '\0';
    jll_info("Follower " STRINGIFY(JL_ROLE) " sending %zu response bytes: %s", bytesWritten, outerSendBuffer);
    Rs485Bus::Get()->WriteMessage(outerSendBuffer, bytesWritten, kBusIdLeader);
  }
#else   // L_BUS_LEADER
  constexpr Milliseconds sFollowerResponseTimeOut = 500;
  static Milliseconds sLastWriteTime = 0;
  static bool sAwaitingResponse = true;
  if (readLength > 0) { sAwaitingResponse = false; }
  if (currentTime - sLastWriteTime < sFollowerResponseTimeOut && sAwaitingResponse) {
    // Too soon to write.
    return;
  }
  sLastWriteTime = currentTime;
  constexpr BusId kFollowerBusIds[] = {4, 5};
  constexpr char* kFollowerNames[] = {"B", "C"};
  constexpr size_t kNumFollowers = sizeof(kFollowerBusIds) / sizeof(kFollowerBusIds[0]);
  static_assert(kNumFollowers == sizeof(kFollowerNames) / sizeof(kFollowerNames[0]), "lengths need to match");
  static size_t sCurrentFollower = kNumFollowers - 1;
  sCurrentFollower++;
  if (sCurrentFollower >= kNumFollowers) { sCurrentFollower = 0; }

  int bytesWritten = snprintf(reinterpret_cast<char*>(outerSendBuffer), kUartBufferSize - 1,
                              "<" STRINGIFY(JL_ROLE) " says at %u : Hey %s what is up?>", currentTime,
                              kFollowerNames[sCurrentFollower]);
  if (bytesWritten >= kUartBufferSize) { bytesWritten = kUartBufferSize - 1; }
  outerSendBuffer[bytesWritten] = '\0';
  jll_info("Leader " STRINGIFY(JL_ROLE) " sending %d bytes: %s", bytesWritten, outerSendBuffer);
  Rs485Bus::Get()->WriteMessage(outerSendBuffer, bytesWritten, kFollowerBusIds[sCurrentFollower]);
  sAwaitingResponse = true;
#endif  // L_BUS_LEADER
}

}  // namespace jazzlights

#endif  // JL_UART
