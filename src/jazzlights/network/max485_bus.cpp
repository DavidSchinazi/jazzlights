#include "jazzlights/network/max485_bus.h"

#if JL_MAX485_BUS

#include <driver/uart.h>
#include <esp_crc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <cstdint>
#include <cstring>
#include <mutex>

#include "jazzlights/esp32_shared.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/buffer.h"
#include "jazzlights/util/cobs.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {

#ifndef JL_LOG_MAX485_BUS_DATA
#define JL_LOG_MAX485_BUS_DATA 0
#endif  // JL_LOG_MAX485_BUS_DATA

#if JL_LOG_MAX485_BUS_DATA
#define jll_max485_data(...) jll_info(__VA_ARGS__)
#define jll_max485_data_buffer(...) jll_buffer_info(__VA_ARGS__)
#else  // JL_LOG_MAX485_BUS_DATA
#define jll_max485_data(...) jll_debug(__VA_ARGS__)
#define jll_max485_data_buffer(...) jll_buffer_debug(__VA_ARGS__)
#endif  // JL_LOG_MAX485_BUS_DATA

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
constexpr size_t kUartDriverBufferSize = 2 * kUartBufferSize;
constexpr uart_event_type_t kApplicationDataAvailable = static_cast<uart_event_type_t>(UART_EVENT_MAX + 1);
static_assert(kApplicationDataAvailable > UART_EVENT_MAX, "UART event types wrapped around");

class Max485BusHandler {
 public:
  static Max485BusHandler* Get();
  ~Max485BusHandler();

  void WriteMessage(const BufferViewU8 message, BusId destBusId);
  void ReadMessage(BufferViewU8* message, BusId* outDestBusId);

  constexpr size_t ComputeExpansion(size_t length) {
    return /*kSeparator=*/1 + /*busID=*/1 + CobsMaxEncodedSize(length) + /*CRC32=*/sizeof(uint32_t) + /*kSeparator=*/1 +
           /*endOfMessage=*/1;
  }

 private:
  explicit Max485BusHandler(uart_port_t uartPort, int txPin, int rxPin, BusId bus_id);
  static void TaskFunction(void* parameters);
  void SetUp();
  void RunTask();
  void WriteData(const BufferViewU8 data);
  void ReadData(BufferViewU8* data);
  void DecodeMessage(const BufferViewU8 encodedBuffer, BufferViewU8* decodedMessage);
  void ShiftEncodedReadBuffer(size_t messageStartIndex);

  const uart_port_t uartPort_;         // Only modified in constructor.
  const int txPin_;                    // Only modified in constructor.
  const int rxPin_;                    // Only modified in constructor.
  const BusId bus_id_;                 // Only modified in constructor.
  TaskHandle_t taskHandle_ = nullptr;  // Only modified in constructor.
  QueueHandle_t queue_ = nullptr;
  std::mutex sendMutex_;
  std::mutex recvMutex_;
  OwnedBufferU8 sharedSendBuffer_;       // Protected by `sendMutex_`.
  size_t lengthInSharedSendBuffer_ = 0;  // Protected by `sendMutex_`.
  OwnedBufferU8 taskSendBuffer_;         // Only accessed by task.
  size_t lengthInTaskSendBuffer_ = 0;    // Only accessed by task.
  OwnedBufferU8 taskRecvBuffer_;         // Only accessed by task.
  OwnedBufferU8 sharedRecvBuffer_;       // Protected by `recvMutex_`.
  size_t lengthInSharedRecvBuffer_ = 0;  // Protected by `recvMutex_`.
  OwnedBufferU8 encodedReadBuffer_;
  size_t encodedReadBufferIndex_ = 0;
  OwnedBufferU8 decodedReadBuffer_;
  size_t decodedReadBufferIndex_ = 0;
};

Max485BusHandler::Max485BusHandler(uart_port_t uartPort, int txPin, int rxPin, BusId bus_id)
    : uartPort_(uartPort),
      txPin_(txPin),
      rxPin_(rxPin),
      bus_id_(bus_id),
      sharedSendBuffer_(kUartBufferSize),
      taskSendBuffer_(kUartBufferSize),
      taskRecvBuffer_(kUartBufferSize),
      sharedRecvBuffer_(kUartBufferSize),
      encodedReadBuffer_(kUartBufferSize),
      decodedReadBuffer_(kUartBufferSize) {
  // Pin task to core 0 to ensure UART interrupts do not get in the way of LED writing on core 1.
  if (xTaskCreatePinnedToCore(TaskFunction, "JL_MAX485_BUS", configMINIMAL_STACK_SIZE + 2000,
                              /*parameters=*/this, kHighestTaskPriority, &taskHandle_,
                              /*coreID=*/0) != pdPASS) {
    jll_fatal("Failed to create Max485BusHandler task");
  }
}

// static
Max485BusHandler* Max485BusHandler::Get() {
  constexpr uart_port_t kUartPort = UART_NUM_2;
  constexpr int kTxPin = 26;
  constexpr int kRxPin = 32;
  static Max485BusHandler sMax485BusHandler(kUartPort, kTxPin, kRxPin, kBusId);
  return &sMax485BusHandler;
}

Max485BusHandler::~Max485BusHandler() { vTaskDelete(taskHandle_); }

// static
void Max485BusHandler::TaskFunction(void* parameters) {
  Max485BusHandler* handler = static_cast<Max485BusHandler*>(parameters);
  // Run setup here instead of in the constructor to ensure interrupts fire on the core that the task is pinned to.
  handler->SetUp();
  while (true) { handler->RunTask(); }
}

void Max485BusHandler::RunTask() {
  uart_event_t event;
  if (!xQueueReceive(queue_, &event, portMAX_DELAY)) { jll_fatal("Max485BusHandler queue error"); }
  switch (event.type) {
    case UART_DATA: {  // There is data ready to be read.
      size_t lengthToRead = std::min<size_t>(event.size, taskRecvBuffer_.size());
      if (lengthToRead > 0) {
        int readLen = uart_read_bytes(uartPort_, &taskRecvBuffer_[0], lengthToRead, portMAX_DELAY);
        if (readLen <= 0) {
          jll_error("UART%d read error %d", uartPort_, readLen);
        } else {
          const std::lock_guard<std::mutex> lock(recvMutex_);
          size_t lengthToCopy = std::min<size_t>(readLen, sharedRecvBuffer_.size() - lengthInSharedRecvBuffer_);
          memcpy(&sharedRecvBuffer_[lengthInSharedRecvBuffer_], &taskRecvBuffer_[0], lengthToCopy);
          lengthInSharedRecvBuffer_ += lengthToCopy;
        }
      }
    } break;
    case UART_BREAK: {  // Received a UART break signal.
      jll_max485_data("UART%d received break signal", uartPort_);
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
        size_t lengthToCopy =
            std::min<size_t>(lengthInSharedSendBuffer_, sharedSendBuffer_.size() - lengthInTaskSendBuffer_);
        memcpy(&taskSendBuffer_[lengthInTaskSendBuffer_], &sharedSendBuffer_[0], lengthToCopy);
        if (lengthToCopy < lengthInSharedSendBuffer_) {
          memmove(&sharedSendBuffer_[0], &sharedSendBuffer_[lengthToCopy], lengthInSharedSendBuffer_ - lengthToCopy);
        }
        lengthInTaskSendBuffer_ += lengthToCopy;
        lengthInSharedSendBuffer_ -= lengthToCopy;
      }
      int bytes_written =
          uart_write_bytes(uartPort_, reinterpret_cast<const char*>(&taskSendBuffer_[0]), lengthInTaskSendBuffer_);
      if (bytes_written > 0) {
        if (static_cast<size_t>(bytes_written) == lengthInTaskSendBuffer_) {
          jll_max485_data("UART%d fully wrote %d bytes", uartPort_, bytes_written);
          lengthInTaskSendBuffer_ = 0;
        } else {
          jll_max485_data("UART%d partially wrote %d/%zu bytes", uartPort_, bytes_written, lengthInTaskSendBuffer_);
          lengthInTaskSendBuffer_ -= bytes_written;
          memmove(&taskSendBuffer_[0], &taskSendBuffer_[bytes_written], lengthInTaskSendBuffer_ - bytes_written);
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

void Max485BusHandler::SetUp() {
  constexpr int kEventQueueSize = 16;
  constexpr int kBaudRate = 115200;
  ESP_ERROR_CHECK(uart_driver_install(uartPort_, kUartDriverBufferSize, kUartDriverBufferSize, kEventQueueSize, &queue_,
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

void Max485BusHandler::WriteData(const BufferViewU8 data) {
  if (data.size() > sharedSendBuffer_.size()) {
    jll_error("UART%d refusing to send %zu bytes > %zu", uartPort_, data.size(), sharedSendBuffer_.size());
    return;
  }
  {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    if (lengthInSharedSendBuffer_ + data.size() > sharedSendBuffer_.size()) {
      jll_error("UART%d cannot currently send %zu bytes, already have %zu and limit is %zu", uartPort_, data.size(),
                lengthInSharedSendBuffer_, sharedSendBuffer_.size());
      return;
    }
    memcpy(&sharedSendBuffer_[lengthInSharedSendBuffer_], &data[0], data.size());
    lengthInSharedSendBuffer_ += data.size();
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

void Max485BusHandler::ReadData(BufferViewU8* data) {
  const std::lock_guard<std::mutex> lock(recvMutex_);
  const size_t readLength = std::min<size_t>(lengthInSharedRecvBuffer_, data->size());
  memcpy(&(*data)[0], &sharedRecvBuffer_[0], readLength);
  lengthInSharedRecvBuffer_ -= readLength;
  data->resize(readLength);
}

void Max485BusHandler::WriteMessage(const BufferViewU8 message, BusId destBusId) {
  if (ComputeExpansion(message.size()) > kUartBufferSize) {
    jll_error("Cannot write message of length %zu", message.size());
    return;
  }
  OwnedBufferU8 uartBuffer1(kUartBufferSize);
  uartBuffer1[0] = kSeparator;
  uartBuffer1[1] = destBusId;
  memcpy(&uartBuffer1[2], &message[0], message.size());
  size_t uartBufferIndex1 = 2 + message.size();
  jll_max485_data_buffer(BufferViewU8(uartBuffer1, 0, uartBufferIndex1), "computing outgoing CRC32");
  uint32_t crc32 = esp_crc32_be(0, &uartBuffer1[0], uartBufferIndex1);
  memcpy(&uartBuffer1[uartBufferIndex1], &crc32, sizeof(crc32));
  uartBufferIndex1 += sizeof(crc32);
  // TODO change the COBS API so we can do this without a second buffer copy.

  OwnedBufferU8 uartBuffer2(kUartBufferSize);
  uartBuffer2[0] = kSeparator;
  uartBuffer2[1] = destBusId;
  BufferViewU8 encodedBuffer(uartBuffer2, 2);
  CobsEncode(BufferViewU8(uartBuffer1, 2, uartBufferIndex1), &encodedBuffer);
  size_t uartBufferIndex2 = 2 + encodedBuffer.size();
  uartBuffer2[uartBufferIndex2++] = kSeparator;
  uartBuffer2[uartBufferIndex2++] = kBusIdEndOfMessage;
  jll_max485_data_buffer(BufferViewU8(uartBuffer2, 0, uartBufferIndex2), "Max485BusHandler writing");
  return WriteData(BufferViewU8(uartBuffer2, 0, uartBufferIndex2));
}

void Max485BusHandler::DecodeMessage(const BufferViewU8 encodedBuffer, BufferViewU8* decodedMessage) {
  jll_max485_data_buffer(encodedBuffer, "DecodeMessage attempting to parse");
  if (encodedBuffer.size() < ComputeExpansion(0)) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage ignoring very short message");
    decodedMessage->resize(0);
    return;
  }
  size_t uartBufferIndex = 0;
  BusId separator = encodedBuffer[uartBufferIndex++];
  if (separator != kSeparator) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage ignoring message due to bad initial separator");
    decodedMessage->resize(0);
    return;
  }
  BusId destBusId = encodedBuffer[uartBufferIndex++];
  if (destBusId != kBusIdBroadcast && destBusId != kBusId) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage ignoring message not meant for us");
    decodedMessage->resize(0);
    return;
  }
  if (encodedBuffer[encodedBuffer.size() - 2] != kSeparator) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage did not find end separator");
    decodedMessage->resize(0);
    return;
  }
  if (encodedBuffer[encodedBuffer.size() - 1] != kBusIdEndOfMessage) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage did not find end of message");
    decodedMessage->resize(0);
    return;
  }
  decodedReadBufferIndex_ = 0;
  decodedReadBuffer_[decodedReadBufferIndex_++] = encodedBuffer[0];
  decodedReadBuffer_[decodedReadBufferIndex_++] = encodedBuffer[1];
  BufferViewU8 decodedBuffer(decodedReadBuffer_, decodedReadBufferIndex_);
  CobsDecode(BufferViewU8(encodedBuffer, uartBufferIndex, encodedBuffer.size() - 2), &decodedBuffer);
  if (decodedBuffer.size() == 0) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage failed to CobsDecode incoming message");
    decodedMessage->resize(0);
    return;
  }
  decodedReadBufferIndex_ += decodedBuffer.size();
  jll_max485_data_buffer(BufferViewU8(decodedReadBuffer_, 0, decodedReadBufferIndex_ - sizeof(uint32_t)),
                         "Max485BusHandler DecodeMessage computing incoming CRC32");
  uint32_t crc32 = esp_crc32_be(0, &decodedReadBuffer_[0], decodedReadBufferIndex_ - sizeof(uint32_t));
  if (memcmp(&decodedReadBuffer_[decodedReadBufferIndex_] - sizeof(crc32), &crc32, sizeof(uint32_t)) != 0) {
    jll_buffer_error(BufferViewU8(decodedReadBuffer_, 0, decodedReadBufferIndex_),
                     "Max485BusHandler DecodeMessage CRC32 check on %d bytes failed",
                     decodedReadBufferIndex_ - sizeof(uint32_t));
    decodedMessage->resize(0);
    return;
  }
  size_t decodedMessageLength = decodedReadBufferIndex_ - 2 - sizeof(uint32_t);
  if (decodedMessageLength > decodedMessage->size()) {
    jll_error("Max485BusHandler DecodeMessage found message too long %zu > %zu", decodedMessageLength,
              decodedMessage->size());
    decodedMessage->resize(0);
    return;
  }
  memcpy(&(*decodedMessage)[0], &decodedReadBuffer_[2], decodedMessageLength);
  decodedMessage->resize(decodedMessageLength);
}

void Max485BusHandler::ShiftEncodedReadBuffer(size_t messageStartIndex) {
  if (messageStartIndex == 0) { return; }
  if (encodedReadBufferIndex_ <= messageStartIndex) {
    encodedReadBufferIndex_ = 0;
    return;
  }
  // Move the message left to the start of the buffer to leave more room for future reads.
  memmove(&encodedReadBuffer_[0], &encodedReadBuffer_[messageStartIndex], encodedReadBufferIndex_ - messageStartIndex);
  encodedReadBufferIndex_ -= messageStartIndex;
}

void Max485BusHandler::ReadMessage(BufferViewU8* message, BusId* outDestBusId) {
  BufferViewU8 encodedReadBuffer(encodedReadBuffer_, encodedReadBufferIndex_);
  ReadData(&encodedReadBuffer);
  if (encodedReadBuffer.size() == 0) {
    message->resize(0);
    return;
  }
  jll_max485_data_buffer(encodedReadBuffer, "Max485BusHandler read");
  encodedReadBufferIndex_ += encodedReadBuffer.size();
  jll_max485_data_buffer(BufferViewU8(encodedReadBuffer_, 0, encodedReadBufferIndex_),
                         "Max485BusHandler data we now have");
  size_t messageStartIndex = 0;
  while (messageStartIndex < encodedReadBufferIndex_) {
    while (messageStartIndex < encodedReadBufferIndex_ && encodedReadBuffer_[messageStartIndex] != kSeparator) {
      messageStartIndex++;
    }
    if (messageStartIndex >= encodedReadBufferIndex_) {
      jll_max485_data_buffer(BufferViewU8(encodedReadBuffer_, 0, encodedReadBufferIndex_),
                             "Max485BusHandler discarding full read buffer without separator");
      encodedReadBufferIndex_ = 0;
      message->resize(0);
      return;
    }
    // Now encodedReadBuffer_[messageStartIndex] is the separator.
    if (encodedReadBufferIndex_ - messageStartIndex < ComputeExpansion(0)) {
      jll_max485_data_buffer(BufferViewU8(encodedReadBuffer_, 0, encodedReadBufferIndex_),
                             "Max485BusHandler pausing because packet too short start=%zu", messageStartIndex);
      ShiftEncodedReadBuffer(messageStartIndex);
      message->resize(0);
      return;
    }
    BusId destBusId = encodedReadBuffer_[messageStartIndex + 1];
    if (destBusId != kBusIdBroadcast && destBusId != kBusId) {
      messageStartIndex++;
      jll_max485_data_buffer(BufferViewU8(encodedReadBuffer_, 0, encodedReadBufferIndex_),
                             "Max485BusHandler skipping past message not meant for us, set start to %zu",
                             messageStartIndex);
      continue;
    }
    // Now we've confirmed that the first two bytes at messageStartIndex indicate a message for us, now to find the end.
    size_t messageEndIndex = messageStartIndex + 2;
    while (messageEndIndex < encodedReadBufferIndex_ && encodedReadBuffer_[messageEndIndex] != kSeparator) {
      messageEndIndex++;
    }
    if (messageEndIndex + 1 >= encodedReadBufferIndex_) {
      jll_max485_data_buffer(BufferViewU8(encodedReadBuffer_, 0, encodedReadBufferIndex_),
                             "Max485BusHandler pausing because packet incomplete");
      ShiftEncodedReadBuffer(messageStartIndex);
      message->resize(0);
      return;
    }
    messageEndIndex++;
    if (encodedReadBuffer_[messageEndIndex] != kBusIdEndOfMessage) {
      jll_max485_data_buffer(BufferViewU8(encodedReadBuffer_, 0, encodedReadBufferIndex_),
                             "Max485BusHandler skipping past message without valid end");
      messageStartIndex = messageEndIndex;
      continue;
    }
    messageEndIndex++;
    BufferViewU8 decodedMessage = *message;
    DecodeMessage(BufferViewU8(encodedReadBuffer_, messageStartIndex, messageEndIndex), &decodedMessage);
    if (decodedMessage.size() == 0) {
      jll_max485_data_buffer(BufferViewU8(encodedReadBuffer_, 0, encodedReadBufferIndex_),
                             "Max485BusHandler skipping past message which failed to decode start=%zu end=%zu",
                             messageStartIndex, messageEndIndex);
      messageStartIndex = messageEndIndex;
      continue;
    }
    ShiftEncodedReadBuffer(messageEndIndex);
    *outDestBusId = destBusId;
    *message = decodedMessage;
    return;
  }
  ShiftEncodedReadBuffer(messageStartIndex);
  message->resize(0);
}

}  // namespace

void SetupMax485Bus(Milliseconds currentTime) { (void)Max485BusHandler::Get(); }

void RunMax485Bus(Milliseconds currentTime) {
  static OwnedBufferU8 outerRecvBuffer(kUartBufferSize);
  static OwnedBufferU8 outerSendBuffer(kUartBufferSize);
  BusId destBusId = kSeparator;
  BufferViewU8 readMessage(outerRecvBuffer);
  Max485BusHandler::Get()->ReadMessage(&readMessage, &destBusId);
  if (readMessage.size() > 0) { jll_buffer_info(readMessage, "UART read message"); }
#if !JL_BUS_LEADER
  if (destBusId == kBusId) {
    outerRecvBuffer[readMessage.size()] = '\0';
    int bytesWritten =
        snprintf(reinterpret_cast<char*>(&outerSendBuffer[0]), outerRecvBuffer.size() - 1,
                 "{" STRINGIFY(JL_ROLE) " is responding at %u to: %s}", currentTime, &outerRecvBuffer[0]);
    if (bytesWritten >= outerRecvBuffer.size()) { bytesWritten = outerRecvBuffer.size() - 1; }
    outerSendBuffer[bytesWritten] = '\0';
    jll_info("Follower " STRINGIFY(JL_ROLE) " sending %d response bytes: %s", bytesWritten, &outerSendBuffer[0]);
    Max485BusHandler::Get()->WriteMessage(BufferViewU8(outerSendBuffer, 0, bytesWritten), kBusIdLeader);
  }
#else   // L_BUS_LEADER
  constexpr Milliseconds sFollowerResponseTimeOut = 500;
  static Milliseconds sLastWriteTime = 0;
  static bool sAwaitingResponse = true;
  if (readMessage.size() > 0) { sAwaitingResponse = false; }
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

  int bytesWritten = snprintf(reinterpret_cast<char*>(&outerSendBuffer[0]), outerSendBuffer.size() - 1,
                              "<" STRINGIFY(JL_ROLE) " says at %u : Hey %s what is up?>", currentTime,
                              kFollowerNames[sCurrentFollower]);
  if (bytesWritten >= outerSendBuffer.size()) { bytesWritten = outerSendBuffer.size() - 1; }
  outerSendBuffer[bytesWritten] = '\0';
  jll_info("Leader " STRINGIFY(JL_ROLE) " sending %d bytes: %s", bytesWritten, &outerSendBuffer[0]);
  Max485BusHandler::Get()->WriteMessage(BufferViewU8(outerSendBuffer, 0, bytesWritten),
                                        kFollowerBusIds[sCurrentFollower]);
  sAwaitingResponse = true;
#endif  // L_BUS_LEADER
}

}  // namespace jazzlights

#endif  // JL_MAX485_BUS
