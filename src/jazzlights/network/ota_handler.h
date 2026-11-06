#ifndef JL_NETWORK_OTA_HANDLER_H
#define JL_NETWORK_OTA_HANDLER_H

#include "jazzlights/util/config.h"

#if JL_OTA

#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include <atomic>

#include "jazzlights/network/secure_channel_server.h"
#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

// Performs firmware updates over the secure channel, see "Part 2: OTA" in extras/docs/OTA_PROPOSAL.md.
class OTAHandler : public SecureChannelHandler {
 public:
  static constexpr uint64_t kFirstType = 3;
  static constexpr uint64_t kLastType = 9;

  explicit OTAHandler(SecureChannelServer* server) : server_(server) {}

  // Sets the device ID that OtaInfo reports. Must be called before the server starts.
  void SetLocalDeviceId(const NetworkDeviceId& localDeviceId) { localDeviceId_ = localDeviceId; }

  // From SecureChannelHandler.
  void OnTlv(SecureSession& session, uint64_t type, const uint8_t* value, uint64_t length) override;
  void OnSessionEnd(SecureSession& session, bool clean) override;

 private:
  void HandleQuery(SecureSession& session);
  void HandleBegin(SecureSession& session, const uint8_t* value, uint64_t length);
  void HandleData(SecureSession& session, const uint8_t* value, uint64_t length);
  void HandleEnd(SecureSession& session);
  void SendAck(SecureSession& session, uint64_t bytesWritten);
  void SendResult(SecureSession& session, uint64_t status);
  void StopUpdate();
  void AbortUpdate();
  // Sends a failed result to the session that's updating, and stops the update.
  void FailUpdate(uint64_t status);

  SecureChannelServer* server_;  // Unowned.
  NetworkDeviceId localDeviceId_;
  SecureSession* updating_ = nullptr;  // The only session that's updating, if any.
  const esp_partition_t* partition_ = nullptr;
  esp_ota_handle_t handle_ = 0;
  mbedtls_sha256_context sha256_;
  uint64_t imageSize_ = 0;
  uint64_t bytesWritten_ = 0;
  uint8_t imageSha256_[32] = {};
  bool rebootWhenDone_ = false;
};

// Decides whether a newly updated firmware is working. If it does, the firmware stays. If it doesn't decide in time,
// including because the primary runloop is stuck, this rolls back to the previous firmware by rebooting.
class OTARollbackGuard {
 public:
  static constexpr Microseconds kDefaultConnectedTime = 30 * kMicrosecondsPerSecond;
  static constexpr Microseconds kDefaultTimeout = 10 * 60 * kMicrosecondsPerSecond;

  explicit OTARollbackGuard(Microseconds connectedTime = kDefaultConnectedTime, Microseconds timeout = kDefaultTimeout)
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
