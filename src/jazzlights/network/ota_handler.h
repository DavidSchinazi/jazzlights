#ifndef JL_NETWORK_OTA_HANDLER_H
#define JL_NETWORK_OTA_HANDLER_H

#include "jazzlights/util/config.h"

#if JL_OTA

#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>

#include <atomic>

#include "jazzlights/network/secure_channel_server.h"
#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

// Performs firmware updates over the secure channel, see "Part 2: OTA" in extras/docs/OTA_PROPOSAL.md.
class OtaHandler : public SecureChannelHandler {
 public:
  static constexpr uint64_t kFirstType = 3;
  static constexpr uint64_t kLastType = 9;

  explicit OtaHandler(SecureChannelServer* server) : server_(server) {}

  // Sets the device ID that OtaInfo reports. Must be called before the server starts.
  void SetLocalDeviceId(const NetworkDeviceId& localDeviceId) { localDeviceId_ = localDeviceId; }

  // Called on the channel's task with true when an update starts, and with false when it stops, however it stops.
  using UpdatingCallback = void (*)(bool updating);
  // Must be called before the server starts.
  void SetUpdatingCallback(UpdatingCallback callback) { updatingCallback_ = callback; }

  // From SecureChannelHandler.
  void OnTlv(SecureSession& session, uint64_t type, const uint8_t* value, uint64_t length) override;
  void OnSessionEnd(SecureSession& session, bool clean) override;

 private:
  void HandleQuery(SecureSession& session);
  void HandleBegin(SecureSession& session, const uint8_t* value, uint64_t length);
  void HandleData(SecureSession& session, const uint8_t* value, uint64_t length);
  void HandleEnd(SecureSession& session);
  void SendAck(SecureSession& session, uint64_t bytesReceived);
  void SendResult(SecureSession& session, uint64_t status);
  bool StartWriter();
  // Waits for the writer task to finish what it's writing, then ends it. Does nothing if it isn't running.
  void StopWriter();
  static void WriterTaskFunction(void* parameters);
  void RunWriterTask();
  void StopUpdate();
  void AbortUpdate();
  // Sends a failed result to the session that's updating, and stops the update.
  void FailUpdate(uint64_t status);

  SecureChannelServer* server_;  // Unowned.
  NetworkDeviceId localDeviceId_;
  UpdatingCallback updatingCallback_ = nullptr;
  SecureSession* updating_ = nullptr;  // The only session that's updating, if any.
  const esp_partition_t* partition_ = nullptr;
  esp_ota_handle_t handle_ = 0;
  mbedtls_sha256_context sha256_;
  uint64_t imageSize_ = 0;
  uint64_t bytesReceived_ = 0;
  // Writing an OtaData to flash takes about as long as receiving it, so during an update a separate task writes each
  // one while this receives the next. These semaphores are created once and kept, so the writer task can't outlive
  // them.
  SemaphoreHandle_t writeReady_ = nullptr;  // Given when writeBuffer_ holds data to write, or writerShouldExit_ is set.
  SemaphoreHandle_t writerIdle_ = nullptr;  // Given by the writer task when it's done with writeBuffer_.
  bool writerRunning_ = false;
  bool writerShouldExit_ = false;
  uint8_t* writeBuffer_ = nullptr;  // Holds kSecureChannelMaxLength bytes while the writer task runs.
  size_t writeSize_ = 0;
  esp_err_t writeError_ = ESP_OK;  // The first error from esp_ota_write() in this update.
  uint8_t imageSha256_[32] = {};
  bool rebootWhenDone_ = false;
};

// Decides whether a newly updated firmware is working. If it does, the firmware stays. If it doesn't decide in time,
// including because the primary runloop is stuck, this rolls back to the previous firmware by rebooting.
class OtaRollbackGuard {
 public:
  static constexpr Microseconds kDefaultConnectedTime = 30 * kMicrosecondsPerSecond;
  static constexpr Microseconds kDefaultTimeout = 10 * 60 * kMicrosecondsPerSecond;

  explicit OtaRollbackGuard(Microseconds connectedTime = kDefaultConnectedTime, Microseconds timeout = kDefaultTimeout)
      : connectedTime_(connectedTime), timeout_(timeout) {}

  // Must be called once at startup, before anything that could get stuck.
  void Start();

  // Called once per primary runloop, saying whether that runloop rendered LEDs and whether the network is connected.
  void RunLoop(bool rendered, bool networkConnected);

 private:
  enum class State {
    kInactive,  // The running firmware doesn't need confirming, or we haven't started.
    kPending,
    kConfirmed,
    kRollingBack,
  };

  static void TimeoutTaskFunction(void* parameters);
  void RunTimeoutTask();

  const Microseconds connectedTime_;
  const Microseconds timeout_;
  // Whichever of the primary runloop and the timeout task moves this out of kPending gets to act on it.
  std::atomic<State> state_{State::kInactive};
  Microseconds deadline_ = 0;            // Only modified before the timeout task starts.
  bool hasRendered_ = false;             // Only used on the primary runloop.
  OptionalMicroseconds connectedSince_;  // Only used on the primary runloop.
};

}  // namespace jazzlights

#endif  // JL_OTA
#endif  // JL_NETWORK_OTA_HANDLER_H
