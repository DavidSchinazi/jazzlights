#ifndef JL_NETWORK_MANAGER_H
#define JL_NETWORK_MANAGER_H

#include <list>
#include <optional>
#include <vector>

#include "jazzlights/network/network.h"
#include "jazzlights/protocol/wire_types.h"

namespace jazzlights {

// NetworkManager owns the list of transports and all the plumbing between them and the synchronization protocol: it
// aggregates the messages received on every transport, fans the message to advertise back out to all of them while
// suppressing echoes, and gives each transport its per-runloop opportunity to send. It owns no protocol state and
// performs no rendering, so networks can be managed without involving the renderer.
class NetworkManager {
 public:
  NetworkManager() = default;

  // Disallow copy and move.
  NetworkManager(const NetworkManager&) = delete;
  NetworkManager(NetworkManager&&) = delete;
  NetworkManager& operator=(const NetworkManager&) = delete;
  NetworkManager& operator=(NetworkManager&&) = delete;

  // Adds a transport. The network is not owned and must outlive this manager.
  void Connect(Network* network);

  // Whether any transport has been connected.
  bool HasNetworks() const { return !networks_.empty(); }

  // Returns the first non-default device ID reported by a transport, or a default-constructed NetworkDeviceId if none
  // of them has one.
  NetworkDeviceId GetLocalDeviceId() const;

  // Gets the messages received on every transport since the last call.
  std::list<ProtocolMessage> GetReceivedMessages();

  // Hands the message to advertise to every transport. Transports that do not echo are told to stop sending when the
  // message was received on them. A nullopt message means there is nothing to advertise and is a no-op.
  void SetMessageToSend(const std::optional<ProtocolMessage>& messageToSend, bool sendAsap = false);

  // Called once per primary runloop, gives every transport an opportunity to send.
  void RunLoop();

 private:
  std::vector<Network*> networks_;  // Unowned.
};

}  // namespace jazzlights
#endif  // JL_NETWORK_MANAGER_H
