#include "jazzlights/network/esp32_socket_manager.h"

#if JL_WIFI || JL_ESP32_ETHERNET

#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <string.h>

#include <algorithm>

#include "jazzlights/util/esp32_shared.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
constexpr size_t kUdpPayloadBufferLength = 1500;
constexpr Microseconds kSendInterval = 100 * kMicrosecondsPerMillisecond;
constexpr UBaseType_t kEventQueueLength = 8;

bool IsAddressSet(const struct in_addr& address) { return address.s_addr != htonl(INADDR_ANY); }
}  // namespace

void Esp32SocketManager::AddInterface(esp_netif_t* netif, NetworkType type, NetworkDeviceId localDeviceId) {
  if (localDeviceId_ == NetworkDeviceId()) { localDeviceId_ = localDeviceId; }
  Event event;
  event.type = Event::Type::kInterfaceAdded;
  event.netif = netif;
  event.networkType = type;
  event.networkId = NextAvailableId();
  jll_info("Esp32SocketManager adding %s interface with NetworkId %u", NetworkTypeToString(type),
           static_cast<unsigned int>(event.networkId));
  PostEvent(event);
}

std::string Esp32SocketManager::GetStatusStr() {
  std::vector<InterfaceStatus> interfaceStatuses;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    interfaceStatuses = interfaceStatuses_;
  }
  std::string statusStr;
  for (const InterfaceStatus& interfaceStatus : interfaceStatuses) {
    if (!IsAddressSet(interfaceStatus.localAddress)) { continue; }
    char addressString[INET_ADDRSTRLEN + 1] = {};
    if (inet_ntop(AF_INET, &interfaceStatus.localAddress, addressString, sizeof(addressString) - 1) == nullptr) {
      jll_fatal("Esp32SocketManager printing local address failed with error %d: %s", errno, strerror(errno));
    }
    if (!statusStr.empty()) { statusStr += ", "; }
    AppendPrintf(&statusStr, "%s %s - %lldms", NetworkTypeToString(interfaceStatus.type), addressString,
                 interfaceStatus.lastReceiveTime ? MsSinceForLogs(*interfaceStatus.lastReceiveTime) : -1);
  }
  if (statusStr.empty()) { return "disconnected"; }
  return statusStr;
}

void Esp32SocketManager::SetMessageToSend(const ProtocolMessage& messageToSend) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void Esp32SocketManager::DisableSending() {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

void Esp32SocketManager::TriggerSendAsap() {
  // TODO: add a loopback socket to this class and include it in the task's poll loop, so the primary runloop can
  // wake up our task when it wants to send ASAP instead of waiting for the poll timeout.
}

std::list<ProtocolMessage> Esp32SocketManager::GetReceivedMessagesImpl() {
  std::list<ProtocolMessage> results;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    results = std::move(receivedMessages_);
    receivedMessages_.clear();
  }
  return results;
}

// static
void Esp32SocketManager::EventHandler(void* eventHandlerArg, esp_event_base_t eventBase, int32_t eventId,
                                      void* eventData) {
  Esp32SocketManager* socketManager = reinterpret_cast<Esp32SocketManager*>(eventHandlerArg);
  socketManager->HandleEvent(eventBase, eventId, eventData);
}

void Esp32SocketManager::HandleEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  if (eventBase != IP_EVENT) { return; }
  if (eventId == IP_EVENT_STA_GOT_IP || eventId == IP_EVENT_ETH_GOT_IP) {
    ip_event_got_ip_t* gotIpEvent = reinterpret_cast<ip_event_got_ip_t*>(eventData);
    Event event;
    event.type = Event::Type::kGotIp;
    event.netif = gotIpEvent->esp_netif;
    memcpy(&event.address, &gotIpEvent->ip_info.ip, sizeof(event.address));
    PostEvent(event);
  } else if (eventId == IP_EVENT_STA_LOST_IP || eventId == IP_EVENT_ETH_LOST_IP) {
    // ESP-IDF uses ip_event_got_ip_t for lost IP events too.
    ip_event_got_ip_t* lostIpEvent = reinterpret_cast<ip_event_got_ip_t*>(eventData);
    Event event;
    event.type = Event::Type::kLostIp;
    event.netif = lostIpEvent->esp_netif;
    PostEvent(event);
  }
}

void Esp32SocketManager::PostEvent(const Event& event) {
  if (xQueueSend(eventQueue_, &event, /*xTicksToWait=*/0) != pdTRUE) {
    jll_error("Esp32SocketManager failed to post event %d because queue is full", static_cast<int>(event.type));
  }
}

void Esp32SocketManager::HandleQueueEvent(const Event& event) {
  if (event.type == Event::Type::kReserved) { jll_fatal("Unexpected Esp32SocketManager::Event::Type::kReserved"); }
  if (event.type == Event::Type::kInterfaceAdded) {
    interfaces_.push_back(Interface{
        .netif = event.netif,
        .type = event.networkType,
        .networkId = event.networkId,
        .localAddress = {},
        .socket = -1,
    });
    {
      const std::lock_guard<std::mutex> lock(mutex_);
      interfaceStatuses_.push_back(InterfaceStatus{
          .type = event.networkType,
          .localAddress = {},
          .lastReceiveTime = std::nullopt,
      });
    }
    const size_t index = interfaces_.size() - 1;
    // The interface may have gotten its address before it was added here, in which case we missed the event.
    esp_netif_ip_info_t ipInfo = {};
    if (esp_netif_get_ip_info(event.netif, &ipInfo) == ESP_OK && ipInfo.ip.addr != 0) {
      jll_info("Esp32SocketManager %s interface already has IP: " IPSTR, NetworkTypeToString(event.networkType),
               IP2STR(&ipInfo.ip));
      struct in_addr localAddress = {};
      memcpy(&localAddress, &ipInfo.ip, sizeof(localAddress));
      SetLocalAddress(index, localAddress);
      CreateSocket(index);
    }
    return;
  }
  for (size_t index = 0; index < interfaces_.size(); index++) {
    if (interfaces_[index].netif != event.netif) { continue; }
    if (event.type == Event::Type::kGotIp) {
      jll_info("Esp32SocketManager %s queue got IP", NetworkTypeToString(interfaces_[index].type));
      SetLocalAddress(index, event.address);
      CreateSocket(index);
    } else if (event.type == Event::Type::kLostIp) {
      jll_info("Esp32SocketManager %s queue lost IP", NetworkTypeToString(interfaces_[index].type));
      SetLocalAddress(index, {});
      CloseSocket(index);
    }
    return;
  }
  // This is an interface we don't manage, or one whose kInterfaceAdded event hasn't been processed yet.
}

void Esp32SocketManager::SetLocalAddress(size_t index, const struct in_addr& localAddress) {
  interfaces_[index].localAddress = localAddress;
  const std::lock_guard<std::mutex> lock(mutex_);
  interfaceStatuses_[index].localAddress = localAddress;
}

bool Esp32SocketManager::HasOpenSocket() const {
  return std::any_of(interfaces_.begin(), interfaces_.end(),
                     [](const Interface& interface) { return interface.socket >= 0; });
}

void Esp32SocketManager::CreateSocket(size_t index) {
  CloseSocket(index);
  Interface& interface = interfaces_[index];
  const char* typeStr = NetworkTypeToString(interface.type);
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    jll_fatal("Esp32SocketManager %s socket() failed with error %d: %s", typeStr, errno, strerror(errno));
  }
  int one = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
    jll_fatal("Esp32SocketManager %s SO_REUSEADDR failed with error %d: %s", typeStr, errno, strerror(errno));
  }
  // Only receive packets from this interface, since every interface has its own socket bound to the same port.
  struct ifreq interfaceRequest = {};
  ESP_ERROR_CHECK(esp_netif_get_netif_impl_name(interface.netif, interfaceRequest.ifr_name));
  if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, &interfaceRequest, sizeof(interfaceRequest)) < 0) {
    jll_fatal("Esp32SocketManager %s SO_BINDTODEVICE to %s failed with error %d: %s", typeStr,
              interfaceRequest.ifr_name, errno, strerror(errno));
  }
  struct sockaddr_in sin = {
      .sin_len = sizeof(struct sockaddr_in),
      .sin_family = AF_INET,
      .sin_port = htons(DefaultUdpPort()),
      .sin_addr = {htonl(INADDR_ANY)},
      .sin_zero = {},
  };
  if (bind(sock, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) != 0) {
    jll_fatal("Esp32SocketManager %s bind() failed with error %d: %s", typeStr, errno, strerror(errno));
  }
  uint8_t ttl = 1;
  if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) != 0) {
    jll_fatal("Esp32SocketManager %s IP_MULTICAST_TTL failed with error %d: %s", typeStr, errno, strerror(errno));
  }
  struct ip_mreq multicastGroup = {
      .imr_multiaddr = multicastAddress_,
      .imr_interface = interface.localAddress,
  };
  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multicastGroup, sizeof(multicastGroup)) < 0) {
    jll_fatal("Esp32SocketManager %s joining multicast failed with error %d: %s", typeStr, errno, strerror(errno));
  }
  if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &interface.localAddress, sizeof(interface.localAddress)) < 0) {
    jll_fatal("Esp32SocketManager %s IP_MULTICAST_IF failed with error %d: %s", typeStr, errno, strerror(errno));
  }
  // Disable receiving our own multicast traffic.
  uint8_t zero = 0;
  if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, &zero, sizeof(zero)) < 0) {
    jll_fatal("Esp32SocketManager %s disabling multicast loopack failed with error %d: %s", typeStr, errno,
              strerror(errno));
  }
  int flags = fcntl(sock, F_GETFL) | O_NONBLOCK;
  if (fcntl(sock, F_SETFL, flags) < 0) {
    jll_fatal("Esp32SocketManager %s setting nonblocking failed with error %d: %s", typeStr, errno, strerror(errno));
  }
  interface.socket = sock;
  jll_info("Esp32SocketManager %s created socket on %s", typeStr, interfaceRequest.ifr_name);
}

void Esp32SocketManager::CloseSocket(size_t index) {
  Interface& interface = interfaces_[index];
  if (interface.socket < 0) { return; }
  jll_info("Esp32SocketManager %s closing socket", NetworkTypeToString(interface.type));
  close(interface.socket);
  interface.socket = -1;
}

void Esp32SocketManager::MaybeSend() {
  bool hasDataToSend;
  ProtocolMessage messageToSend;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    hasDataToSend = hasDataToSend_;
    if (hasDataToSend) { messageToSend = messageToSend_; }
  }
  if (!hasDataToSend) { return; }
  const Microseconds currentTime = TimeMicros();
  if (lastSendTime_ && currentTime - *lastSendTime_ < kSendInterval &&
      messageToSend.currentPattern == lastSentPattern_) {
    return;
  }
  lastSendTime_ = currentTime;
  lastSentPattern_ = messageToSend.currentPattern;
  const size_t payloadLength = WriteUdpPayload(messageToSend, udpPayload_, kUdpPayloadBufferLength);
  if (payloadLength == 0) {
    jll_error("Esp32SocketManager failed to write");
    return;
  }
  struct sockaddr_in sin = {
      .sin_len = sizeof(struct sockaddr_in),
      .sin_family = AF_INET,
      .sin_port = htons(DefaultUdpPort()),
      .sin_addr = multicastAddress_,
      .sin_zero = {},
  };
  for (const Interface& interface : interfaces_) {
    if (interface.socket < 0) { continue; }
    // Don't echo a message back onto the interface where we received it.
    if (interface.networkId == messageToSend.receiptNetworkId) { continue; }
    ssize_t writeRes = sendto(interface.socket, udpPayload_, payloadLength, /*flags=*/0,
                              reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    (void)writeRes;
  }
}

void Esp32SocketManager::Receive(size_t index) {
  Interface& interface = interfaces_[index];
  while (interface.socket >= 0) {
    sockaddr_in sin = {};
    socklen_t sinLength = sizeof(sin);
    ssize_t n = recvfrom(interface.socket, udpPayload_, kUdpPayloadBufferLength, /*flags=*/0,
                         reinterpret_cast<sockaddr*>(&sin), &sinLength);
    if (n < 0) {
      static_assert(EWOULDBLOCK == EAGAIN, "need to handle these separately");
      if (errno == EWOULDBLOCK) { return; }
      jll_error("Esp32SocketManager %s recvfrom failed with error %d: %s", NetworkTypeToString(interface.type), errno,
                strerror(errno));
      CreateSocket(index);
      return;
    }
    char addressString[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &(sin.sin_addr), addressString, sizeof(addressString)) == nullptr) {
      jll_fatal("Esp32SocketManager printing receive address failed with error %d: %s", errno, strerror(errno));
    }
    std::string receiptDetails;
    AppendPrintf(&receiptDetails, " (from %s:%u)", addressString, ntohs(sin.sin_port));
    ProtocolMessage receivedMessage;
    if (!ParseUdpPayload(udpPayload_, n, receiptDetails, &receivedMessage)) { continue; }
    receivedMessage.receiptNetworkId = interface.networkId;
    receivedMessage.receiptNetworkType = interface.type;
    const Microseconds receiveTime = TimeMicros();
    lastReceiveTime_.store(receiveTime, std::memory_order_relaxed);
    const std::lock_guard<std::mutex> lock(mutex_);
    interfaceStatuses_[index].lastReceiveTime = receiveTime;
    receivedMessages_.push_back(receivedMessage);
  }
}

void Esp32SocketManager::WaitForReceiveOrSendTime() {
  pollFds_.clear();
  for (const Interface& interface : interfaces_) {
    if (interface.socket < 0) { continue; }
    pollFds_.push_back(pollfd{
        .fd = interface.socket,
        .events = POLLIN,
        .revents = 0,
    });
  }
  int pollTimeoutMs = static_cast<int>(kSendInterval / kMicrosecondsPerMillisecond);
  if (lastSendTime_) {
    const int timeSinceLastSendMs = static_cast<int>((TimeMicros() - *lastSendTime_) / kMicrosecondsPerMillisecond);
    if (timeSinceLastSendMs <= pollTimeoutMs) { pollTimeoutMs -= timeSinceLastSendMs; }
  }
  // Data available and timeouts both just restart the loop, which will receive and send as needed.
  if (poll(pollFds_.data(), pollFds_.size(), pollTimeoutMs) < 0) {
    jll_error("Esp32SocketManager poll failed with error %d: %s", errno, strerror(errno));
    for (size_t index = 0; index < interfaces_.size(); index++) {
      if (interfaces_[index].socket >= 0) { CreateSocket(index); }
    }
  }
}

// static
void Esp32SocketManager::TaskFunction(void* parameters) {
  Esp32SocketManager* socketManager = reinterpret_cast<Esp32SocketManager*>(parameters);
  while (true) { socketManager->RunTask(); }
}

void Esp32SocketManager::RunTask() {
  Event event;
  while (xQueueReceive(eventQueue_, &event, /*xTicksToWait=*/0)) { HandleQueueEvent(event); }
  if (!HasOpenSocket()) {
    // Wait until an interface gets an address.
    jll_info("Esp32SocketManager waiting for queue event");
    if (xQueueReceive(eventQueue_, &event, portMAX_DELAY)) { HandleQueueEvent(event); }
    return;  // Restart loop.
  }
  MaybeSend();
  for (size_t index = 0; index < interfaces_.size(); index++) { Receive(index); }
  WaitForReceiveOrSendTime();
}

Esp32SocketManager::Esp32SocketManager()
    : eventQueue_(xQueueCreate(/*numQueueItems=*/kEventQueueLength, /*queueItemSize=*/sizeof(Event))) {
  if (eventQueue_ == nullptr) { jll_fatal("Failed to create Esp32SocketManager queue"); }
  udpPayload_ = reinterpret_cast<uint8_t*>(malloc(kUdpPayloadBufferLength));
  if (udpPayload_ == nullptr) {
    jll_fatal("Esp32SocketManager failed to allocate UDP payload buffer of length %zu", kUdpPayloadBufferLength);
  }
  if (inet_pton(AF_INET, DefaultMulticastAddress(), &multicastAddress_) != 1) {
    jll_fatal("Esp32SocketManager failed to parse multicast address");
  }
  InitializeNetStack();
  esp_event_handler_instance_t ipInstance;
  // Register event handler on the default event loop. That runs in task "sys_evt" on core 0 (and is not
  // configurable).
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &ipInstance));

  // This task is pinned to core 0 since that's where the system event handler runs (see above).
  if (xTaskCreatePinnedToCore(TaskFunction, "JL_Sockets", configMINIMAL_STACK_SIZE + 2000,
                              /*parameters=*/this, kHighestTaskPriority, &taskHandle_, /*coreID=*/0) != pdPASS) {
    jll_fatal("Failed to create Esp32SocketManager task");
  }
  jll_info("Esp32SocketManager initialized");
}

// static
Esp32SocketManager* Esp32SocketManager::Get() {
  static Esp32SocketManager sSingleton;
  return &sSingleton;
}

Esp32SocketManager::~Esp32SocketManager() {
  jll_fatal("Destructing Esp32SocketManager is not currently supported");
  // This destruction is unsafe since a race condition could cause the event handler to fire after the destructor is
  // called. If we ever have a need to destroy this, we'll need to make this safe first.
}

}  // namespace jazzlights

#endif  // JL_WIFI || JL_ESP32_ETHERNET
