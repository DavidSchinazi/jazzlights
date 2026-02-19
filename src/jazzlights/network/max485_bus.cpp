#include "jazzlights/network/max485_bus.h"

#if JL_MAX485_BUS

#include <driver/uart.h>
#include <esp_crc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <atomic>
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

#if JL_BUS_LEADER
#define JL_BUS_ID kBusIdLeader
#elif !defined(JL_BUS_ID)
#error "Followers need to define JL_BUS_ID"
#endif

constexpr BusId kBusIdSelf = (JL_BUS_ID);

// While most bus ID values are available, we're reserving values above 128 for future use.
static_assert(kBusIdSelf < 128, "high bus ID");
static_assert(kBusIdSelf >= kBusIdLeader, "low bus ID");
#if !JL_BUS_LEADER
static_assert(kBusIdSelf != kBusIdLeader, "follower bus ID cannot be equal to leader");
#endif  // JL_BUS_LEADER

std::vector<BusId> GetFollowers() {
#if JL_BUS_LEADER
  return {4, 5};
#else   // JL_BUS_LEADER
  return {};
#endif  // JL_BUS_LEADER
}

constexpr size_t ComputeExpansion(size_t length) {
  return /*separator*/ 1 + /*destBusID*/ 1 + /*srcBusID*/ 1 + CobsMaxEncodedSize(length + /*CRC32*/ sizeof(uint32_t)) +
         /*separator*/ 1 + /*endOfMessage*/ 1;
}

constexpr size_t kMaxMessageLength = 1000;
constexpr size_t kMaxEncodedMessageLength = ComputeExpansion(kMaxMessageLength);  // 1013.
constexpr size_t kUartDriverBufferSize = 2048;
static_assert(kUartDriverBufferSize >= 2 * kMaxEncodedMessageLength, "bad size");

constexpr uart_event_type_t kApplicationDataAvailable = static_cast<uart_event_type_t>(UART_EVENT_MAX + 1);
static_assert(kApplicationDataAvailable > UART_EVENT_MAX, "UART event types wrapped around");

class Max485BusHandler {
 public:
  static Max485BusHandler* Get();
  ~Max485BusHandler();

  void SetMessageToSend(const BufferViewU8 message);
  BufferViewU8 ReadMessage(OwnedBufferU8& readMessageBuffer, BusId* destBusId, BusId* srcBusId);

 private:
  explicit Max485BusHandler(uart_port_t uartPort, int txPin, int rxPin, BusId busIdSelf,
                            const std::vector<BusId>& followers);
  static void TaskFunction(void* parameters);
  void SetUp();
  void RunTask();
  static BufferViewU8 DecodeMessage(const BufferViewU8 encodedBuffer, OwnedBufferU8& decodedReadBuffer,
                                    OwnedBufferU8& decodedMessageBuffer, BusId busIdSelf);
  void ShiftTaskRecvBuffer(size_t messageStartIndex);
  BufferViewU8 TaskFindReceivedMessage(BusId* destBusId, BusId* srcBusId);
  void SendToUart(BufferViewU8 encodedData);
  static BufferViewU8 EncodeMessage(const BufferViewU8 message, OwnedBufferU8& encodedMessageBuffer, BusId destBusId,
                                    BusId srcBusId);
  void CopyEncodeAndSendMessage(BusId destBusId);
  void SendMessageToNextFollower();
  bool IsReady() const;

  const uart_port_t uartPort_;          // Only modified in constructor.
  const int txPin_;                     // Only modified in constructor.
  const int rxPin_;                     // Only modified in constructor.
  const BusId busIdSelf_;               // Only modified in constructor.
  const std::vector<BusId> followers_;  // Only modified in constructor.
  TaskHandle_t taskHandle_ = nullptr;   // Only modified in constructor.
  QueueHandle_t queue_ = nullptr;
  std::atomic<bool> ready_{false};
  std::mutex sendMutex_;
  OwnedBufferU8 sharedSendMessageBuffer_;                // Protected by `sendMutex_`.
  BufferViewU8 sharedSendMessage_;                       // Protected by `sendMutex_`.
  size_t nextFollowerIndex_ = 0;                         // Only accessed by task.
  OwnedBufferU8 taskSendMessageBuffer_;                  // Only accessed by task.
  OwnedBufferU8 taskEncodedSendMessageBuffer_;           // Only accessed by task.
  Milliseconds taskLastSendTimeExpectingResponse_ = -1;  // Only accessed by task.
  std::mutex recvMutex_;
  OwnedBufferU8 taskRecvBuffer_;                    // Only accessed by task.
  size_t lengthInTaskRecvBuffer_ = 0;               // Only accessed by task.
  OwnedBufferU8 taskDecodedReadBuffer_;             // Only accessed by task.
  OwnedBufferU8 taskDecodedMessageBuffer_;          // Only accessed by task.
  OwnedBufferU8 sharedRecvSelfMessageBuffer_;       // Protected by `recvMutex_`.
  BufferViewU8 sharedRecvSelfMessage_;              // Protected by `recvMutex_`.
  BusId sharedRecvSelfSender_ = kSeparator;         // Protected by `recvMutex_`.
  OwnedBufferU8 sharedRecvBroadcastMessageBuffer_;  // Protected by `recvMutex_`.
  BufferViewU8 sharedRecvBroadcastMessage_;         // Protected by `recvMutex_`.
  BusId sharedRecvBroadcastSender_ = kSeparator;    // Protected by `recvMutex_`.
};

bool Max485BusHandler::IsReady() const { return ready_.load(std::memory_order_relaxed); }

Max485BusHandler::Max485BusHandler(uart_port_t uartPort, int txPin, int rxPin, BusId busIdSelf,
                                   const std::vector<BusId>& followers)
    : uartPort_(uartPort),
      txPin_(txPin),
      rxPin_(rxPin),
      busIdSelf_(busIdSelf),
      followers_(followers),
      sharedSendMessageBuffer_(kMaxMessageLength),
      taskSendMessageBuffer_(kMaxMessageLength),
      taskEncodedSendMessageBuffer_(kMaxEncodedMessageLength),
      taskRecvBuffer_(kUartDriverBufferSize),
      taskDecodedMessageBuffer_(kMaxMessageLength),
      sharedRecvSelfMessageBuffer_(kMaxMessageLength),
      sharedRecvBroadcastMessageBuffer_(kMaxMessageLength),
      taskDecodedReadBuffer_(kMaxEncodedMessageLength) {
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
  static Max485BusHandler sMax485BusHandler(kUartPort, kTxPin, kRxPin, kBusIdSelf, GetFollowers());
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

void Max485BusHandler::SendToUart(BufferViewU8 encodedData) {
  int bytes_written = uart_write_bytes(uartPort_, reinterpret_cast<const char*>(&encodedData[0]), encodedData.size());
  if (bytes_written > 0) {
    if (static_cast<size_t>(bytes_written) == encodedData.size()) {
      jll_max485_data("UART%d fully wrote %d bytes", uartPort_, bytes_written);
    } else {
      jll_buffer_error(encodedData, "UART%d partially wrote %d bytes", uartPort_, bytes_written);
      // If this happens in practice we'll need to handle partial writes more gracefully.
    }
  } else {
    jll_buffer_error(encodedData, "UART%d got error %d trying to write", uartPort_, bytes_written);
  }
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
          lengthInTaskRecvBuffer_ = readLen;
          BusId destBusId = kSeparator;
          BusId srcBusId = kSeparator;
          while (true) {
            BufferViewU8 taskMessageView = TaskFindReceivedMessage(&destBusId, &srcBusId);
            if (taskMessageView.empty()) { break; }
            jll_buffer_info(taskMessageView, STRINGIFY(JL_ROLE) " received from %d to %d", static_cast<int>(srcBusId),
                            static_cast<int>(destBusId));
            if (destBusId == busIdSelf_) {
              {
                const std::lock_guard<std::mutex> lock(recvMutex_);
                sharedRecvSelfMessage_ = sharedRecvSelfMessageBuffer_.CopyIn(taskMessageView);
                sharedRecvSelfSender_ = srcBusId;
              }
              if (busIdSelf_ == kBusIdLeader) {
                SendMessageToNextFollower();
              } else if (srcBusId == kBusIdLeader) {  // Send response.
                CopyEncodeAndSendMessage(kBusIdLeader);
              }
            } else if (destBusId == kBusIdBroadcast) {
              const std::lock_guard<std::mutex> lock(recvMutex_);
              sharedRecvBroadcastMessage_ = sharedRecvBroadcastMessageBuffer_.CopyIn(taskMessageView);
              sharedRecvBroadcastSender_ = srcBusId;
            } else {
              jll_fatal("Unexpected bus ID %d", static_cast<int>(destBusId));
            }
          }
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
      constexpr Milliseconds kUartResponseTimeout = 500;
      bool shouldSend = false;
      if (taskLastSendTimeExpectingResponse_ < 0) {
        jll_info("Initiating first send");
        shouldSend = true;
      } else if (timeMillis() - taskLastSendTimeExpectingResponse_ > kUartResponseTimeout) {
        jll_info("Timed out waiting for response");
        shouldSend = true;
      } else {
        jll_max485_data("Ignoring kApplicationDataAvailable");
      }
      if (shouldSend) { SendMessageToNextFollower(); }
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
      .source_clk = UART_SCLK_APB,
  };
  ESP_ERROR_CHECK(uart_param_config(uartPort_, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(uartPort_, txPin_, rxPin_, /*rts=*/UART_PIN_NO_CHANGE, /*cts=*/UART_PIN_NO_CHANGE));
  ready_.store(true, std::memory_order_relaxed);
}

// static
BufferViewU8 Max485BusHandler::EncodeMessage(const BufferViewU8 message, OwnedBufferU8& encodedMessageBuffer,
                                             BusId destBusId, BusId srcBusId) {
  if (message.size() > kMaxMessageLength) {
    jll_error("Cannot encode message of length %zu", message.size());
    return BufferViewU8();
  }
  encodedMessageBuffer[0] = kSeparator;
  encodedMessageBuffer[1] = destBusId;
  encodedMessageBuffer[2] = srcBusId;
  jll_max485_data_buffer(BufferViewU8(encodedMessageBuffer, 0, 3), "computing outgoing CRC32 1/2");
  uint32_t crc32 = esp_crc32_be(0, &encodedMessageBuffer[0], 3);
  jll_max485_data_buffer(message, "computing outgoing CRC32 2/2");
  crc32 = esp_crc32_be(crc32, &message[0], message.size());
  BufferViewU8 cobsInput[2] = {message, BufferViewU8(reinterpret_cast<uint8_t*>(&crc32), sizeof(crc32))};
  BufferViewU8 encodedBuffer(encodedMessageBuffer, 3);
  CobsEncode(cobsInput, 2, &encodedBuffer);
  encodedMessageBuffer[3 + encodedBuffer.size()] = kSeparator;
  encodedMessageBuffer[3 + encodedBuffer.size() + 1] = kBusIdEndOfMessage;
  BufferViewU8 encodedMessage(encodedMessageBuffer, 0, 3 + encodedBuffer.size() + 2);
  jll_max485_data_buffer(encodedMessage, "Max485BusHandler encoded message");
  return encodedMessage;
}

void Max485BusHandler::CopyEncodeAndSendMessage(BusId destBusId) {
  BufferViewU8 taskSendMessage;
  {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    taskSendMessage = taskSendMessageBuffer_.CopyIn(sharedSendMessage_);
  }
  jll_buffer_info(taskSendMessage, STRINGIFY(JL_ROLE) " sending from %d to %d", static_cast<int>(busIdSelf_),
                  static_cast<int>(destBusId));
  BufferViewU8 taskEncodedSendMessage =
      EncodeMessage(taskSendMessage, taskEncodedSendMessageBuffer_, destBusId, busIdSelf_);
  if (!taskEncodedSendMessage.empty()) {
    SendToUart(taskEncodedSendMessage);
    if (destBusId != kBusIdBroadcast) { taskLastSendTimeExpectingResponse_ = timeMillis(); }
  }
}

void Max485BusHandler::SendMessageToNextFollower() {
  if (followers_.empty()) { return; }
  BusId destBusId = followers_[nextFollowerIndex_];
  nextFollowerIndex_++;
  if (nextFollowerIndex_ >= followers_.size()) { nextFollowerIndex_ = 0; }
  CopyEncodeAndSendMessage(destBusId);
}

void Max485BusHandler::SetMessageToSend(const BufferViewU8 message) {
  if (message.size() > kMaxMessageLength) {
    jll_error("UART%d refusing to send message of length %zu", uartPort_, message.size());
    return;
  }
  {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    sharedSendMessage_ = sharedSendMessageBuffer_.CopyIn(message);
  }
  if (busIdSelf_ != kBusIdLeader) { return; }
  uart_event_t eventToSend = {};
  eventToSend.type = kApplicationDataAvailable;
  BaseType_t res = xQueueSendToBack(queue_, &eventToSend, /*ticksToWait=*/0);
  if (res == errQUEUE_FULL) {
    jll_error("UART%d event queue is full", uartPort_);
  } else if (res != pdPASS) {
    jll_error("UART%d event queue error %d", uartPort_, res);
  }
}

// static
BufferViewU8 Max485BusHandler::DecodeMessage(const BufferViewU8 encodedBuffer, OwnedBufferU8& decodedReadBuffer,
                                             OwnedBufferU8& decodedMessageBuffer, BusId busIdSelf) {
  jll_max485_data_buffer(encodedBuffer, "DecodeMessage attempting to parse");
  if (encodedBuffer.size() < ComputeExpansion(0)) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage ignoring very short message");
    return BufferViewU8();
  }
  BusId separator = encodedBuffer[0];
  if (separator != kSeparator) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage ignoring message due to bad initial separator");
    return BufferViewU8();
  }
  BusId destBusId = encodedBuffer[1];
  if (destBusId != kBusIdBroadcast && destBusId != busIdSelf) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage ignoring message not meant for us");
    return BufferViewU8();
  }
  if (encodedBuffer[encodedBuffer.size() - 2] != kSeparator) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage did not find end separator");
    return BufferViewU8();
  }
  if (encodedBuffer[encodedBuffer.size() - 1] != kBusIdEndOfMessage) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage did not find end of message");
    return BufferViewU8();
  }
  decodedReadBuffer[0] = encodedBuffer[0];
  decodedReadBuffer[1] = encodedBuffer[1];
  decodedReadBuffer[2] = encodedBuffer[2];
  BufferViewU8 decodedBuffer(decodedReadBuffer, 3);
  CobsDecode(BufferViewU8(encodedBuffer, 3, encodedBuffer.size() - 2), &decodedBuffer);
  if (decodedBuffer.size() == 0) {
    jll_buffer_error(encodedBuffer, "Max485BusHandler DecodeMessage failed to CobsDecode incoming message");
    return BufferViewU8();
  }
  size_t decodedReadBufferIndex = 3 + decodedBuffer.size();
  jll_max485_data_buffer(BufferViewU8(decodedReadBuffer, 0, decodedReadBufferIndex - sizeof(uint32_t)),
                         "Max485BusHandler DecodeMessage computing incoming CRC32");
  uint32_t crc32 = esp_crc32_be(0, &decodedReadBuffer[0], decodedReadBufferIndex - sizeof(uint32_t));
  if (memcmp(&decodedReadBuffer[decodedReadBufferIndex] - sizeof(crc32), &crc32, sizeof(uint32_t)) != 0) {
    jll_buffer_error(BufferViewU8(decodedReadBuffer, 0, decodedReadBufferIndex),
                     "Max485BusHandler DecodeMessage CRC32 check on %d bytes failed",
                     decodedReadBufferIndex - sizeof(uint32_t));
    return BufferViewU8();
  }
  size_t decodedMessageLength = decodedReadBufferIndex - 3 - sizeof(uint32_t);
  if (decodedMessageLength > decodedMessageBuffer.size()) {
    jll_error("Max485BusHandler DecodeMessage found message too long %zu > %zu", decodedMessageLength,
              decodedMessageBuffer.size());
    return BufferViewU8();
  }
  return decodedMessageBuffer.CopyIn(BufferViewU8(decodedReadBuffer, 3, decodedMessageLength + 3));
}

void Max485BusHandler::ShiftTaskRecvBuffer(size_t messageStartIndex) {
  if (messageStartIndex == 0) { return; }
  if (lengthInTaskRecvBuffer_ <= messageStartIndex) {
    lengthInTaskRecvBuffer_ = 0;
    return;
  }
  // Move the message left to the start of the buffer to leave more room for future reads.
  memmove(&taskRecvBuffer_[0], &taskRecvBuffer_[messageStartIndex], lengthInTaskRecvBuffer_ - messageStartIndex);
  lengthInTaskRecvBuffer_ -= messageStartIndex;
}

BufferViewU8 Max485BusHandler::ReadMessage(OwnedBufferU8& readMessageBuffer, BusId* destBusId, BusId* srcBusId) {
  if (!IsReady()) { return BufferViewU8(); }
  const std::lock_guard<std::mutex> lock(recvMutex_);
  if (!sharedRecvSelfMessage_.empty()) {
    *destBusId = busIdSelf_;
    *srcBusId = sharedRecvSelfSender_;
    BufferViewU8 ret = readMessageBuffer.CopyIn(sharedRecvSelfMessage_);
    sharedRecvSelfMessage_ = BufferViewU8();
    return ret;
  } else if (!sharedRecvBroadcastMessage_.empty()) {
    *destBusId = kBusIdBroadcast;
    *srcBusId = sharedRecvBroadcastSender_;
    BufferViewU8 ret = readMessageBuffer.CopyIn(sharedRecvBroadcastMessage_);
    sharedRecvBroadcastMessage_ = BufferViewU8();
    return ret;
  } else {
    return BufferViewU8();
  }
}

BufferViewU8 Max485BusHandler::TaskFindReceivedMessage(BusId* destBusId, BusId* srcBusId) {
  jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                         "Max485BusHandler TaskFindReceivedMessage");
  size_t messageStartIndex = 0;
  while (messageStartIndex < lengthInTaskRecvBuffer_) {
    while (messageStartIndex < lengthInTaskRecvBuffer_ && taskRecvBuffer_[messageStartIndex] != kSeparator) {
      messageStartIndex++;
    }
    if (messageStartIndex >= lengthInTaskRecvBuffer_) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "Max485BusHandler discarding full task read buffer without separator");
      lengthInTaskRecvBuffer_ = 0;
      return BufferViewU8();
    }
    // Now taskRecvBuffer_[messageStartIndex] is the separator.
    if (lengthInTaskRecvBuffer_ - messageStartIndex < ComputeExpansion(0)) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "Max485BusHandler pausing because task message too short start=%zu", messageStartIndex);
      ShiftTaskRecvBuffer(messageStartIndex);
      return BufferViewU8();
    }
    *destBusId = taskRecvBuffer_[messageStartIndex + 1];
    if (*destBusId != kBusIdBroadcast && *destBusId != busIdSelf_) {
      messageStartIndex++;
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "Max485BusHandler task skipping past message not meant for us, set start to %zu",
                             messageStartIndex);
      continue;
    }
    *srcBusId = taskRecvBuffer_[messageStartIndex + 2];
    // We've confirmed that the first two bytes at messageStartIndex indicate a message for us, now to find the end.
    size_t messageEndIndex = messageStartIndex + 3;
    while (messageEndIndex < lengthInTaskRecvBuffer_ && taskRecvBuffer_[messageEndIndex] != kSeparator) {
      messageEndIndex++;
    }
    if (messageEndIndex + 1 >= lengthInTaskRecvBuffer_) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "Max485BusHandler task pausing because packet incomplete");
      ShiftTaskRecvBuffer(messageStartIndex);
      return BufferViewU8();
    }
    messageEndIndex++;
    if (taskRecvBuffer_[messageEndIndex] != kBusIdEndOfMessage) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "Max485BusHandler task  skipping past message without valid end");
      messageStartIndex = messageEndIndex;
      continue;
    }
    messageEndIndex++;
    BufferViewU8 decodedMessage = DecodeMessage(BufferViewU8(taskRecvBuffer_, messageStartIndex, messageEndIndex),
                                                taskDecodedReadBuffer_, taskDecodedMessageBuffer_, busIdSelf_);
    if (decodedMessage.empty()) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "Max485BusHandler skipping past message which failed to decode start=%zu end=%zu",
                             messageStartIndex, messageEndIndex);
      messageStartIndex = messageEndIndex;
      continue;
    }
    ShiftTaskRecvBuffer(messageEndIndex);
    return decodedMessage;
  }
  ShiftTaskRecvBuffer(messageStartIndex);
  return BufferViewU8();
}

}  // namespace

void SetupMax485Bus(Milliseconds currentTime) { (void)Max485BusHandler::Get(); }

void RunMax485Bus(Milliseconds currentTime) {
  static OwnedBufferU8 outerRecvBuffer(kMaxMessageLength);
  static OwnedBufferU8 outerSendBuffer(kMaxMessageLength);
  BusId destBusId = kSeparator;
  BusId srcBusId = kSeparator;
  BufferViewU8 readMessage = Max485BusHandler::Get()->ReadMessage(outerRecvBuffer, &destBusId, &srcBusId);
  if (readMessage.size() > 0) {
    jll_buffer_info(readMessage, STRINGIFY(JL_ROLE) " outer read message from %d to %d", static_cast<int>(srcBusId),
                    static_cast<int>(destBusId));
  }
  if (kBusIdSelf != kBusIdLeader) {  // We are a follower.
    if (destBusId == kBusIdSelf) {
      outerRecvBuffer[readMessage.size()] = '\0';
      int bytesWritten =
          snprintf(reinterpret_cast<char*>(&outerSendBuffer[0]), outerRecvBuffer.size() - 1,
                   "{" STRINGIFY(JL_ROLE) " is responding at %u to: %s}", currentTime, &outerRecvBuffer[0]);
      if (bytesWritten >= outerRecvBuffer.size()) { bytesWritten = outerRecvBuffer.size() - 1; }
      outerSendBuffer[bytesWritten] = '\0';
      Max485BusHandler::Get()->SetMessageToSend(BufferViewU8(outerSendBuffer, 0, bytesWritten));
    }
  } else {  // We are the leader.
    int bytesWritten = snprintf(reinterpret_cast<char*>(&outerSendBuffer[0]), outerSendBuffer.size() - 1,
                                "<" STRINGIFY(JL_ROLE) " says it is %u.>", currentTime);
    if (bytesWritten >= outerSendBuffer.size()) { bytesWritten = outerSendBuffer.size() - 1; }
    outerSendBuffer[bytesWritten] = '\0';
    Max485BusHandler::Get()->SetMessageToSend(BufferViewU8(outerSendBuffer, 0, bytesWritten));
  }
}

}  // namespace jazzlights

#endif  // JL_MAX485_BUS
