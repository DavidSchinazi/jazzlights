#ifndef JL_NETWORK_OTA_HANDLER_H
#define JL_NETWORK_OTA_HANDLER_H

#include "jazzlights/util/config.h"

#if JL_OTA

#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>

#include "jazzlights/network/secure_channel_server.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

// Performs firmware updates over the secure channel, see "Part 2: OTA" in extras/docs/OTA_PROPOSAL.md.
class OTAHandler : public SecureChannelHandler {
 public:
  static constexpr uint64_t kFirstType = 3;
  static constexpr uint64_t kLastType = 9;

  explicit OTAHandler(SecureChannelServer* server) : server_(server) {}

  // From SecureChannelHandler.
  void OnTLV(SecureSession& session, uint64_t type, const uint8_t* value, size_t length) override;
  void OnSessionEnd(SecureSession& session, bool clean) override;

 private:
  void HandleQuery(SecureSession& session);
  void HandleBegin(SecureSession& session, const uint8_t* value, size_t length);
  void HandleData(SecureSession& session, const uint8_t* value, size_t length);
  void HandleEnd(SecureSession& session);
  void SendAck(SecureSession& session, uint64_t bytesWritten);
  void SendResult(SecureSession& session, uint64_t status);
  void StopUpdate();
  void AbortUpdate();
  // Sends a failed result to the session that's updating, and stops the update.
  void FailUpdate(uint64_t status);

  SecureChannelServer* server_;        // Unowned.
  SecureSession* updating_ = nullptr;  // The only session that's updating, if any.
  const esp_partition_t* partition_ = nullptr;
  esp_ota_handle_t handle_ = 0;
  mbedtls_sha256_context sha256_;
  uint64_t imageSize_ = 0;
  uint64_t bytesWritten_ = 0;
  uint8_t imageSha256_[32] = {};
  bool rebootWhenDone_ = false;
};

// Decides when a newly updated firmware is working, so the bootloader doesn't roll it back on the next boot. Only used
// on the primary runloop.
class OTARollbackGuard {
 public:
  // Called once per primary runloop, saying whether that runloop rendered LEDs and whether the network is connected.
  void RunLoop(bool rendered, bool networkConnected);

 private:
  bool checkedState_ = false;
  bool done_ = false;
  bool hasRendered_ = false;
  OptionalMicroseconds connectedSince_;
};

}  // namespace jazzlights

#endif  // JL_OTA
#endif  // JL_NETWORK_OTA_HANDLER_H
