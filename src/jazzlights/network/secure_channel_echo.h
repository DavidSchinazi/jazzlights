#ifndef JL_NETWORK_SECURE_CHANNEL_ECHO_H
#define JL_NETWORK_SECURE_CHANNEL_ECHO_H

#include "jazzlights/network/secure_channel_server.h"

#if JL_SECURE_CHANNEL && JL_SECURE_CHANNEL_ECHO

namespace jazzlights {

// Sends every Echo TLV back as is, which allows testing the channel without touching flash. Only for test builds.
class SecureChannelEchoHandler : public SecureChannelHandler {
 public:
  static constexpr uint64_t kEchoType = 1000;

  void OnTLV(SecureSession& session, uint64_t type, const uint8_t* value, size_t length) override {
    session.Send(type, value, length);
  }
  void OnSessionEnd(SecureSession& /*session*/, bool /*clean*/) override {}
};

}  // namespace jazzlights

#endif  // JL_SECURE_CHANNEL && JL_SECURE_CHANNEL_ECHO
#endif  // JL_NETWORK_SECURE_CHANNEL_ECHO_H
