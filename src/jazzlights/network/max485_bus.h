#ifndef JL_NETWORK_MAX485_BUS_H
#define JL_NETWORK_MAX485_BUS_H

#include "jazzlights/config.h"

#if JL_MAX485_BUS

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

#include "jazzlights/util/buffer.h"
#include "jazzlights/util/cobs.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

using BusId = uint8_t;

class Max485BusHandler {
 public:
  static inline constexpr BusId kBusIdEndOfMessage = 1;
  static inline constexpr BusId kBusIdBroadcast = 2;
  static inline constexpr BusId kBusIdLeader = 3;

  virtual ~Max485BusHandler();

  BufferViewU8 ReadMessage(OwnedBufferU8& readMessageBuffer, BusId* destBusId, BusId* srcBusId);

  BusId GetBusIdSelf() const { return busIdSelf_.load(std::memory_order_relaxed); }

 protected:
  explicit Max485BusHandler(uart_port_t uartPort, int txPin, int rxPin, BusId busIdSelf);

  bool SetMessageToSendInner(BusId destBusId, const BufferViewU8 message);
  virtual void HandleReceivedMessage(BusId srcBusId, const BufferViewU8 message) = 0;
  virtual void HandleApplicationDataAvailableToSend() = 0;
  void CopyEncodeAndSendMessage(BusId destBusId);

  inline static constexpr uint8_t kSeparator = 0;
  static void TaskFunction(void* parameters);
  void Setup();
  void RunTask();
  static BufferViewU8 DecodeMessage(const BufferViewU8 encodedBuffer, OwnedBufferU8& decodedReadBuffer,
                                    OwnedBufferU8& decodedMessageBuffer, BusId busIdSelf);
  void ShiftTaskRecvBuffer(size_t messageStartIndex);
  BufferViewU8 TaskFindReceivedMessage(BusId* destBusId, BusId* srcBusId);
  void SendToUart(BufferViewU8 encodedData);
  static BufferViewU8 EncodeMessage(const BufferViewU8 message, OwnedBufferU8& encodedMessageBuffer, BusId destBusId,
                                    BusId srcBusId);
  bool IsReady() const;

  const uart_port_t uartPort_;         // Only modified in constructor.
  const int txPin_;                    // Only modified in constructor.
  const int rxPin_;                    // Only modified in constructor.
  TaskHandle_t taskHandle_ = nullptr;  // Only modified in constructor.
  QueueHandle_t queue_ = nullptr;
  std::atomic<BusId> busIdSelf_;
  std::atomic<bool> ready_{false};
  std::mutex sendMutex_;
  std::map<BusId, OwnedBufferU8> sharedSendMessageBuffers_;  // Protected by `sendMutex_`.
  std::map<BusId, BufferViewU8> sharedSendMessages_;         // Protected by `sendMutex_`.
  OwnedBufferU8 taskSendMessageBuffer_;                      // Only accessed by task.
  OwnedBufferU8 taskEncodedSendMessageBuffer_;               // Only accessed by task.
  Milliseconds taskLastSendTimeExpectingResponse_ = -1;      // Only accessed by task.
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

  static inline constexpr uart_event_type_t kApplicationDataAvailable =
      static_cast<uart_event_type_t>(UART_EVENT_MAX + 1);
};

class Max485BusLeader : public Max485BusHandler {
 public:
  explicit Max485BusLeader(uart_port_t uartPort, int txPin, int rxPin);
  void SetMessageToSend(BusId destBusId, const BufferViewU8 message);

 protected:
  void HandleReceivedMessage(BusId srcBusId, const BufferViewU8 message) override;
  void HandleApplicationDataAvailableToSend() override;

 private:
  void SendMessageToNextFollower();
  std::deque<BusId> highPriorityBusIds_;  // Protected by `sendMutex_`.
  BusId lastSentBusId_ = 0;               // Only accessed by task.
};

class Max485BusFollower : public Max485BusHandler {
 public:
  explicit Max485BusFollower(uart_port_t uartPort, int txPin, int rxPin, BusId busIdSelf);
  void SetMessageToSend(const BufferViewU8 message);
  void SetBusIdSelf(BusId busIdSelf) { busIdSelf_.store(busIdSelf, std::memory_order_relaxed); }

 protected:
  void HandleReceivedMessage(BusId srcBusId, const BufferViewU8 message) override;
  void HandleApplicationDataAvailableToSend() override;
};

}  // namespace jazzlights

#endif  // JL_MAX485_BUS

#endif  // JL_NETWORK_MAX485_BUS_H
