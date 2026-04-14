#include "jazzlights/network/max485_bus.h"

#if JL_MAX485_BUS

#include <driver/uart.h>
#include <esp_crc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>

#include "jazzlights/esp32_shared.h"
#include "jazzlights/orrery_planet.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/buffer.h"
#include "jazzlights/util/cobs.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

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

#ifndef JL_TEST_MAX485
#define JL_TEST_MAX485 0
#endif  // JL_TEST_MAX485

constexpr size_t kMaxMessageLength = 1000;

constexpr size_t ComputeExpansion(size_t length) {
  return /*separator*/ 1 + /*destBusID*/ 1 + /*srcBusID*/ 1 + CobsMaxEncodedSize(length + /*CRC32*/ sizeof(uint32_t)) +
         /*separator*/ 1 + /*endOfMessage*/ 1;
}

constexpr size_t kMaxEncodedMessageLength = ComputeExpansion(kMaxMessageLength);  // 1013.
constexpr size_t kUartDriverBufferSize = 2048;

bool Max485BusHandler::IsReady() const { return ready_.load(std::memory_order_relaxed); }

Max485BusHandler::Max485BusHandler(uart_port_t uartPort, int txPin, int rxPin, BusId busIdSelf)
    : uartPort_(uartPort),
      txPin_(txPin),
      rxPin_(rxPin),
      busIdSelf_(busIdSelf),
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
    jll_fatal("%u Failed to create Max485BusHandler task", timeMillis());
  }
}

Max485BusHandler::~Max485BusHandler() { vTaskDelete(taskHandle_); }

// static
void Max485BusHandler::TaskFunction(void* parameters) {
  Max485BusHandler* handler = static_cast<Max485BusHandler*>(parameters);
  // Run setup here instead of in the constructor to ensure interrupts fire on the core that the task is pinned to.
  handler->Setup();
  while (true) { handler->RunTask(); }
}

void Max485BusHandler::SendToUart(BufferViewU8 encodedData) {
  int bytes_written = uart_write_bytes(uartPort_, reinterpret_cast<const char*>(&encodedData[0]), encodedData.size());
  if (bytes_written > 0) {
    if (static_cast<size_t>(bytes_written) == encodedData.size()) {
      jll_max485_data("%u UART%d fully wrote %d bytes", timeMillis(), uartPort_, bytes_written);
    } else {
      jll_buffer_error(encodedData, "%u UART%d partially wrote %d bytes", timeMillis(), uartPort_, bytes_written);
      // If this happens in practice we'll need to handle partial writes more gracefully.
    }
  } else {
    jll_buffer_error(encodedData, "%u UART%d got error %d trying to write", timeMillis(), uartPort_, bytes_written);
  }
}

void Max485BusHandler::RunTask() {
  uart_event_t event;
  if (!xQueueReceive(queue_, &event, portMAX_DELAY)) { jll_fatal("%u Max485BusHandler queue error", timeMillis()); }
  switch (event.type) {
    case UART_DATA: {  // There is data ready to be read.
      size_t lengthToRead = std::min<size_t>(event.size, taskRecvBuffer_.size());
      if (lengthToRead > 0) {
        int readLen = uart_read_bytes(uartPort_, &taskRecvBuffer_[0], lengthToRead, portMAX_DELAY);
        if (readLen <= 0) {
          jll_error("%u UART%d read error %d", timeMillis(), uartPort_, readLen);
        } else {
          lengthInTaskRecvBuffer_ = readLen;
          BusId destBusId = kSeparator;
          BusId srcBusId = kSeparator;
          while (true) {
            BufferViewU8 taskMessageView = TaskFindReceivedMessage(&destBusId, &srcBusId);
            if (taskMessageView.empty()) { break; }
            jll_buffer_info(taskMessageView, "%u Received from %d to %d", timeMillis(), static_cast<int>(srcBusId),
                            static_cast<int>(destBusId));
            if (destBusId == GetBusIdSelf()) {
              {
                const std::lock_guard<std::mutex> lock(recvMutex_);
                sharedRecvSelfMessage_ = sharedRecvSelfMessageBuffer_.CopyIn(taskMessageView);
                sharedRecvSelfSender_ = srcBusId;
              }
              HandleReceivedMessage(srcBusId, taskMessageView);
            } else if (destBusId == kBusIdBroadcast) {
              const std::lock_guard<std::mutex> lock(recvMutex_);
              sharedRecvBroadcastMessage_ = sharedRecvBroadcastMessageBuffer_.CopyIn(taskMessageView);
              sharedRecvBroadcastSender_ = srcBusId;
            } else {
              jll_fatal("%u Unexpected bus ID %d", timeMillis(), static_cast<int>(destBusId));
            }
          }
        }
      }
    } break;
    case UART_BREAK: {  // Received a UART break signal.
      jll_max485_data("%u UART%d received break signal", timeMillis(), uartPort_);
    } break;
    case UART_BUFFER_FULL: {  // The hardware receive buffer is full.
      jll_error("%u UART%d read full", timeMillis(), uartPort_);
    } break;
    case UART_FIFO_OVF: {  // The hardware receive buffer has overflowed.
      jll_error("%u UART%d read overflow", timeMillis(), uartPort_);
      ESP_ERROR_CHECK(uart_flush_input(uartPort_));
    } break;
    case UART_FRAME_ERR: {  // Received byte with data frame error.
      jll_error("%u UART%d data frame error", timeMillis(), uartPort_);
    } break;
    case UART_PARITY_ERR: {  // Received byte with parity error.
      jll_error("%u UART%d parity error", timeMillis(), uartPort_);
    } break;
    case UART_DATA_BREAK: {  // Sent a UART break signal.
      jll_info("%u UART%d sent break signal", timeMillis(), uartPort_);
    } break;
    case UART_PATTERN_DET: {  // Detected pattern in received data.
      jll_error("%u UART%d pattern detected", timeMillis(), uartPort_);
    } break;
    case kApplicationDataAvailable: {  // Our application has data to write to UART.
      HandleApplicationDataAvailableToSend();
    } break;
    default: {
      jll_info("%u UART%d unexpected event %d", timeMillis(), uartPort_, static_cast<int>(event.type));
    } break;
  }
}

void Max485BusHandler::Setup() {
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
    jll_error("%u Cannot encode message of length %zu", timeMillis(), message.size());
    return BufferViewU8();
  }
  encodedMessageBuffer[0] = kSeparator;
  encodedMessageBuffer[1] = destBusId;
  encodedMessageBuffer[2] = srcBusId;
  jll_max485_data_buffer(BufferViewU8(encodedMessageBuffer, 0, 3), "%u computing outgoing CRC32 1/2", timeMillis());
  uint32_t crc32 = esp_crc32_be(0, &encodedMessageBuffer[0], 3);
  jll_max485_data_buffer(message, "%u computing outgoing CRC32 2/2", timeMillis());
  crc32 = esp_crc32_be(crc32, &message[0], message.size());
  BufferViewU8 cobsInput[2] = {message, BufferViewU8(reinterpret_cast<uint8_t*>(&crc32), sizeof(crc32))};
  BufferViewU8 encodedBuffer(encodedMessageBuffer, 3);
  CobsEncode(cobsInput, 2, &encodedBuffer);
  encodedMessageBuffer[3 + encodedBuffer.size()] = kSeparator;
  encodedMessageBuffer[3 + encodedBuffer.size() + 1] = kBusIdEndOfMessage;
  BufferViewU8 encodedMessage(encodedMessageBuffer, 0, 3 + encodedBuffer.size() + 2);
  jll_max485_data_buffer(encodedMessage, "%u Max485BusHandler encoded message", timeMillis());
  return encodedMessage;
}

void Max485BusHandler::CopyEncodeAndSendMessage(BusId destBusId) {
  BufferViewU8 taskSendMessage;
  {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    auto it = sharedSendMessages_.find(destBusId);
    if (it == sharedSendMessages_.end()) { it = sharedSendMessages_.find(kBusIdBroadcast); }
    if (it != sharedSendMessages_.end()) { taskSendMessage = taskSendMessageBuffer_.CopyIn(it->second); }
  }
  if (taskSendMessage.empty()) { return; }
  const BusId busIdSelf = GetBusIdSelf();
  jll_buffer_info(taskSendMessage, "%u Sending from %d to %d", timeMillis(), static_cast<int>(busIdSelf),
                  static_cast<int>(destBusId));
  BufferViewU8 taskEncodedSendMessage =
      EncodeMessage(taskSendMessage, taskEncodedSendMessageBuffer_, destBusId, busIdSelf);
  if (!taskEncodedSendMessage.empty()) {
    SendToUart(taskEncodedSendMessage);
    if (destBusId != kBusIdBroadcast) { taskLastSendTimeExpectingResponse_ = timeMillis(); }
  }
}

bool Max485BusHandler::SetMessageToSendInner(BusId destBusId, const BufferViewU8 message) {
  if (message.size() > kMaxMessageLength) {
    jll_error("%u UART%d refusing to send message of length %zu", timeMillis(), uartPort_, message.size());
    return false;
  }
  const std::lock_guard<std::mutex> lock(sendMutex_);
  auto it = sharedSendMessages_.find(destBusId);
  bool isNew = false;
  if (it == sharedSendMessages_.end()) {
    isNew = true;
  } else if (it->second.size() != message.size() || memcmp(it->second.data(), message.data(), message.size()) != 0) {
    isNew = true;
  }

  if (isNew) {
    if (sharedSendMessageBuffers_.find(destBusId) == sharedSendMessageBuffers_.end()) {
      sharedSendMessageBuffers_.emplace(destBusId, OwnedBufferU8(kMaxMessageLength));
    }
    sharedSendMessages_[destBusId] = sharedSendMessageBuffers_.at(destBusId).CopyIn(message);
  }
  return isNew;
}

// static
BufferViewU8 Max485BusHandler::DecodeMessage(const BufferViewU8 encodedBuffer, OwnedBufferU8& decodedReadBuffer,
                                             OwnedBufferU8& decodedMessageBuffer, BusId busIdSelf) {
  jll_max485_data_buffer(encodedBuffer, "%u DecodeMessage attempting to parse", timeMillis());
  if (encodedBuffer.size() < ComputeExpansion(0)) {
    jll_buffer_error(encodedBuffer, "%u Max485BusHandler DecodeMessage ignoring very short message", timeMillis());
    return BufferViewU8();
  }
  BusId separator = encodedBuffer[0];
  if (separator != kSeparator) {
    jll_buffer_error(encodedBuffer, "%u Max485BusHandler DecodeMessage ignoring message due to bad initial separator",
                     timeMillis());
    return BufferViewU8();
  }
  BusId destBusId = encodedBuffer[1];
  if (destBusId != kBusIdBroadcast && destBusId != busIdSelf) {
    jll_buffer_error(encodedBuffer, "%u Max485BusHandler DecodeMessage ignoring message not meant for us",
                     timeMillis());
    return BufferViewU8();
  }
  if (encodedBuffer[encodedBuffer.size() - 2] != kSeparator) {
    jll_buffer_error(encodedBuffer, "%u Max485BusHandler DecodeMessage did not find end separator", timeMillis());
    return BufferViewU8();
  }
  if (encodedBuffer[encodedBuffer.size() - 1] != kBusIdEndOfMessage) {
    jll_buffer_error(encodedBuffer, "%u Max485BusHandler DecodeMessage did not find end of message", timeMillis());
    return BufferViewU8();
  }
  decodedReadBuffer[0] = encodedBuffer[0];
  decodedReadBuffer[1] = encodedBuffer[1];
  decodedReadBuffer[2] = encodedBuffer[2];
  BufferViewU8 decodedBuffer(decodedReadBuffer, 3);
  CobsDecode(BufferViewU8(encodedBuffer, 3, encodedBuffer.size() - 2), &decodedBuffer);
  if (decodedBuffer.size() == 0) {
    jll_buffer_error(encodedBuffer, "%u Max485BusHandler DecodeMessage failed to CobsDecode incoming message",
                     timeMillis());
    return BufferViewU8();
  }
  size_t decodedReadBufferIndex = 3 + decodedBuffer.size();
  jll_max485_data_buffer(BufferViewU8(decodedReadBuffer, 0, decodedReadBufferIndex - sizeof(uint32_t)),
                         "%u Max485BusHandler DecodeMessage computing incoming CRC32", timeMillis());
  uint32_t crc32 = esp_crc32_be(0, &decodedReadBuffer[0], decodedReadBufferIndex - sizeof(uint32_t));
  if (memcmp(&decodedReadBuffer[decodedReadBufferIndex] - sizeof(crc32), &crc32, sizeof(uint32_t)) != 0) {
    jll_buffer_error(BufferViewU8(decodedReadBuffer, 0, decodedReadBufferIndex),
                     "%u Max485BusHandler DecodeMessage CRC32 check on %d bytes failed", timeMillis(),
                     decodedReadBufferIndex - sizeof(uint32_t));
    return BufferViewU8();
  }
  size_t decodedMessageLength = decodedReadBufferIndex - 3 - sizeof(uint32_t);
  if (decodedMessageLength > decodedMessageBuffer.size()) {
    jll_error("%u Max485BusHandler DecodeMessage found message too long %zu > %zu", timeMillis(), decodedMessageLength,
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
    *destBusId = GetBusIdSelf();
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
                         "%u Max485BusHandler TaskFindReceivedMessage", timeMillis());
  size_t messageStartIndex = 0;
  while (messageStartIndex < lengthInTaskRecvBuffer_) {
    while (messageStartIndex < lengthInTaskRecvBuffer_ && taskRecvBuffer_[messageStartIndex] != kSeparator) {
      messageStartIndex++;
    }
    if (messageStartIndex >= lengthInTaskRecvBuffer_) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "%u Max485BusHandler discarding full task read buffer without separator", timeMillis());
      lengthInTaskRecvBuffer_ = 0;
      return BufferViewU8();
    }
    // Now taskRecvBuffer_[messageStartIndex] is the separator.
    if (lengthInTaskRecvBuffer_ - messageStartIndex < ComputeExpansion(0)) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "%u Max485BusHandler pausing because task message too short start=%zu", timeMillis(),
                             messageStartIndex);
      ShiftTaskRecvBuffer(messageStartIndex);
      return BufferViewU8();
    }
    *destBusId = taskRecvBuffer_[messageStartIndex + 1];
    const BusId busIdSelf = GetBusIdSelf();
    if (*destBusId != kBusIdBroadcast && *destBusId != busIdSelf) {
      messageStartIndex++;
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "%u Max485BusHandler task skipping past message not meant for us, set start to %zu",
                             timeMillis(), messageStartIndex);
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
                             "%u Max485BusHandler task pausing because packet incomplete", timeMillis());
      ShiftTaskRecvBuffer(messageStartIndex);
      return BufferViewU8();
    }
    messageEndIndex++;
    if (taskRecvBuffer_[messageEndIndex] != kBusIdEndOfMessage) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "%u Max485BusHandler task  skipping past message without valid end", timeMillis());
      messageStartIndex = messageEndIndex;
      continue;
    }
    messageEndIndex++;
    BufferViewU8 decodedMessage = DecodeMessage(BufferViewU8(taskRecvBuffer_, messageStartIndex, messageEndIndex),
                                                taskDecodedReadBuffer_, taskDecodedMessageBuffer_, busIdSelf);
    if (decodedMessage.empty()) {
      jll_max485_data_buffer(BufferViewU8(taskRecvBuffer_, 0, lengthInTaskRecvBuffer_),
                             "%u Max485BusHandler skipping past message which failed to decode start=%zu end=%zu",
                             timeMillis(), messageStartIndex, messageEndIndex);
      messageStartIndex = messageEndIndex;
      continue;
    }
    ShiftTaskRecvBuffer(messageEndIndex);
    return decodedMessage;
  }
  ShiftTaskRecvBuffer(messageStartIndex);
  return BufferViewU8();
}

Max485BusLeader::Max485BusLeader(uart_port_t uartPort, int txPin, int rxPin)
    : Max485BusHandler(uartPort, txPin, rxPin, kBusIdLeader) {}

void Max485BusLeader::HandleReceivedMessage(BusId /*srcBusId*/, const BufferViewU8 /*message*/) {
  SendMessageToNextFollower();
}

void Max485BusLeader::HandleApplicationDataAvailableToSend() {
  constexpr Milliseconds kUartResponseTimeout = 500;
  bool shouldSend = false;
  if (taskLastSendTimeExpectingResponse_ < 0) {
    jll_info("%u Initiating first send", timeMillis());
    shouldSend = true;
  } else if (timeMillis() - taskLastSendTimeExpectingResponse_ > kUartResponseTimeout) {
    jll_info("%u Timed out waiting for response", timeMillis());
    shouldSend = true;
  } else {
    jll_max485_data("%u Ignoring kApplicationDataAvailable", timeMillis());
  }
  if (shouldSend) { SendMessageToNextFollower(); }
}

void Max485BusLeader::SendMessageToNextFollower() {
  BusId destBusId = kSeparator;
  {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    if (!highPriorityBusIds_.empty()) {
      destBusId = highPriorityBusIds_.front();
      highPriorityBusIds_.pop_front();
      lastSentBusId_ = destBusId;
    } else {
      if (sharedSendMessages_.empty()) { return; }
      auto it = sharedSendMessages_.upper_bound(lastSentBusId_);
      if (it == sharedSendMessages_.end()) { it = sharedSendMessages_.begin(); }
      destBusId = it->first;
      lastSentBusId_ = destBusId;
    }
  }
  if (destBusId != kSeparator) { CopyEncodeAndSendMessage(destBusId); }
}

void Max485BusLeader::SetMessageToSend(BusId destBusId, const BufferViewU8 message) {
  if (SetMessageToSendInner(destBusId, message)) {
    const std::lock_guard<std::mutex> lock(sendMutex_);
    if (std::find(highPriorityBusIds_.begin(), highPriorityBusIds_.end(), destBusId) == highPriorityBusIds_.end()) {
      highPriorityBusIds_.push_back(destBusId);
    }
  }
  uart_event_t eventToSend = {};
  eventToSend.type = kApplicationDataAvailable;
  BaseType_t res = xQueueSendToBack(queue_, &eventToSend, /*ticksToWait=*/0);
  if (res == errQUEUE_FULL) {
    jll_error("%u UART%d event queue is full", timeMillis(), uartPort_);
  } else if (res != pdPASS) {
    jll_error("%u UART%d event queue error %d", timeMillis(), uartPort_, res);
  }
}

Max485BusFollower::Max485BusFollower(uart_port_t uartPort, int txPin, int rxPin, BusId busIdSelf)
    : Max485BusHandler(uartPort, txPin, rxPin, busIdSelf) {}

void Max485BusFollower::HandleReceivedMessage(BusId srcBusId, const BufferViewU8 /*message*/) {
  if (srcBusId == kBusIdLeader) { CopyEncodeAndSendMessage(kBusIdLeader); }
}

void Max485BusFollower::HandleApplicationDataAvailableToSend() {}

void Max485BusFollower::SetMessageToSend(const BufferViewU8 message) { SetMessageToSendInner(kBusIdLeader, message); }

}  // namespace jazzlights

#endif  // JL_MAX485_BUS
