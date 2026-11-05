#ifndef JL_NETWORK_SECURE_CHANNEL_KEYS_H
#define JL_NETWORK_SECURE_CHANNEL_KEYS_H

#include "jazzlights/network/secure_channel_server.h"

#if JL_SECURE_CHANNEL

#include <array>

namespace jazzlights {

// The public keys of the clients that may use the secure channel, as 65-byte uncompressed P-256 points. They all have
// the same permissions. To add a client, see "Keys" in extras/docs/OTA_PROPOSAL.md, and update the count below. The
// server doesn't start while this list is empty.
inline constexpr std::array<SecureChannelPublicKey, 0> kSecureChannelPublicKeys = {};

}  // namespace jazzlights

#endif  // JL_SECURE_CHANNEL
#endif  // JL_NETWORK_SECURE_CHANNEL_KEYS_H
