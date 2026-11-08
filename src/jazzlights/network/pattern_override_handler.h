#ifndef JL_NETWORK_PATTERN_OVERRIDE_HANDLER_H
#define JL_NETWORK_PATTERN_OVERRIDE_HANDLER_H

#include "jazzlights/network/secure_channel_server.h"

#if JL_SECURE_CHANNEL

#include <optional>

#include "jazzlights/util/log.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

// Lets a client show one pattern on this device's LEDs, instead of the synchronized ones, see "PatternOverride" in
// extras/docs/OTA.md. A 4-byte Value holds the pattern bits, in network byte order, and an empty one goes back
// to the synchronized patterns. The override also ends when the session that set it ends, so that a client that goes
// away can't leave a device stuck on its pattern.
class PatternOverrideHandler : public SecureChannelHandler {
 public:
  static constexpr uint64_t kPatternOverrideType = 10;

  // Called on the channel's task with the pattern to show, or with nullopt to go back to the synchronized patterns.
  using OverrideCallback = void (*)(std::optional<PatternBits> pattern);
  // Must be called before the server starts.
  void SetOverrideCallback(OverrideCallback callback) { overrideCallback_ = callback; }

  // From SecureChannelHandler.
  void OnTlv(SecureSession& session, uint64_t /*type*/, const uint8_t* value, uint64_t length) override {
    if (length == 0) {
      StopOverride();
    } else if (length == 4) {
      const PatternBits pattern = static_cast<PatternBits>(value[0]) << 24 | static_cast<PatternBits>(value[1]) << 16 |
                                  static_cast<PatternBits>(value[2]) << 8 | static_cast<PatternBits>(value[3]);
      jll_info("PatternOverride showing pattern 0x%08x", static_cast<unsigned>(pattern));
      overriding_ = &session;
      if (overrideCallback_ != nullptr) { overrideCallback_(pattern); }
    } else {
      session.Close(kSecureChannelStatusMalformed);
    }
  }

  void OnSessionEnd(SecureSession& session, bool /*clean*/) override {
    if (overriding_ == &session) { StopOverride(); }
  }

 private:
  void StopOverride() {
    if (overriding_ == nullptr) { return; }
    jll_info("PatternOverride stopping");
    overriding_ = nullptr;
    if (overrideCallback_ != nullptr) { overrideCallback_(std::nullopt); }
  }

  OverrideCallback overrideCallback_ = nullptr;
  SecureSession* overriding_ = nullptr;  // The session that set the override, if there is one.
};

}  // namespace jazzlights

#endif  // JL_SECURE_CHANNEL
#endif  // JL_NETWORK_PATTERN_OVERRIDE_HANDLER_H
