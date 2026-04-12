#ifndef JL_NETWORK_MAX485_BUS_H
#define JL_NETWORK_MAX485_BUS_H

#include "jazzlights/config.h"

#if JL_MAX485_BUS

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <atomic>
#include <mutex>
#include <vector>

#include "jazzlights/util/buffer.h"
#include "jazzlights/util/cobs.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

using BusId = uint8_t;

BusId BusIdSelf();
std::vector<BusId> GetFollowers();

class Max485BusHandler {
 public:
  explicit Max485BusHandler(uart_port_t uartPort, int txPin, int rxPin, BusId busIdSelf,
                            const std::vector<BusId>& followers);
  ~Max485BusHandler();

  void SetMessageToSend(const BufferViewU8 message);
  BufferViewU8 ReadMessage(OwnedBufferU8& readMessageBuffer, BusId* destBusId, BusId* srcBusId);

 private:
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
}  // namespace jazzlights

#endif  // JL_MAX485_BUS

#endif  // JL_NETWORK_MAX485_BUS_H
