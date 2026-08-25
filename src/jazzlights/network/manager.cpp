#include "jazzlights/network/manager.h"

#include "jazzlights/util/log.h"

#ifndef JL_PLAYER_LOG_MESSAGES
#define JL_PLAYER_LOG_MESSAGES 0
#endif  // JL_PLAYER_LOG_MESSAGES

#if JL_PLAYER_LOG_MESSAGES
#define jll_player_message(...) jll_info(__VA_ARGS__)
#else  // JL_PLAYER_LOG_MESSAGES
#define jll_player_message(...) jll_debug(__VA_ARGS__)
#endif  // JL_PLAYER_LOG_MESSAGES

namespace jazzlights {

void NetworkManager::Connect(Network* network) {
  jll_info("Connecting network %s", NetworkTypeToString(network->Type()));
  networks_.push_back(network);
}

NetworkDeviceId NetworkManager::GetLocalDeviceId() const {
  for (const Network* network : networks_) {
    NetworkDeviceId localDeviceId = network->GetLocalDeviceId();
    if (localDeviceId != NetworkDeviceId()) { return localDeviceId; }
  }
  return NetworkDeviceId();
}

std::list<ProtocolMessage> NetworkManager::GetReceivedMessages() {
  std::list<ProtocolMessage> receivedMessages;
  for (Network* network : networks_) {
    std::list<ProtocolMessage> messagesFromNetwork = network->GetReceivedMessages();
    receivedMessages.splice(receivedMessages.end(), messagesFromNetwork);
  }
  return receivedMessages;
}

void NetworkManager::SetMessageToSend(const std::optional<ProtocolMessage>& messageToSend, bool sendAsap) {
  if (!messageToSend) { return; }
  for (Network* network : networks_) {
    if (!network->ShouldEcho() && messageToSend->receiptNetworkId == network->id()) {
      jll_debug("Not echoing for %s to %s ", NetworkTypeToString(network->Type()),
                NetworkMessageToString(*messageToSend).c_str());
      network->DisableSending();
      continue;
    }
    jll_player_message("Setting messageToSend for %s to %s ", NetworkTypeToString(network->Type()),
                       NetworkMessageToString(*messageToSend).c_str());
    network->SetMessageToSend(*messageToSend);
  }
  if (sendAsap) {
    for (Network* network : networks_) { network->TriggerSendAsap(); }
  }
}

void NetworkManager::RunLoop() {
  for (Network* network : networks_) { network->RunLoop(); }
}

}  // namespace jazzlights
