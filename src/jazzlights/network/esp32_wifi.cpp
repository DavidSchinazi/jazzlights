#include "jazzlights/network/esp32_wifi.h"

#if JL_WIFI

#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <string.h>

#include <sstream>

#include "jazzlights/util/esp32_shared.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/pseudorandom.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
constexpr size_t kReceiveBufferLength = 1500;
constexpr Microseconds kSendInterval = 100 * kMicrosecondsPerMillisecond;
constexpr uint32_t kNumReconnectsBeforeDelay = 10;

std::string WiFiReasonToString(uint8_t reason) {
  switch (reason) {
#define JL_WIFI_REASON_CASE_RETURN(r) \
  case WIFI_REASON_##r: return #r;
    JL_WIFI_REASON_CASE_RETURN(UNSPECIFIED)
    JL_WIFI_REASON_CASE_RETURN(AUTH_EXPIRE)
    JL_WIFI_REASON_CASE_RETURN(AUTH_LEAVE)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_EXPIRE)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_TOOMANY)
    JL_WIFI_REASON_CASE_RETURN(NOT_AUTHED)
    JL_WIFI_REASON_CASE_RETURN(NOT_ASSOCED)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_LEAVE)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_NOT_AUTHED)
    JL_WIFI_REASON_CASE_RETURN(DISASSOC_PWRCAP_BAD)
    JL_WIFI_REASON_CASE_RETURN(DISASSOC_SUPCHAN_BAD)
    JL_WIFI_REASON_CASE_RETURN(BSS_TRANSITION_DISASSOC)
    JL_WIFI_REASON_CASE_RETURN(IE_INVALID)
    JL_WIFI_REASON_CASE_RETURN(MIC_FAILURE)
    JL_WIFI_REASON_CASE_RETURN(4WAY_HANDSHAKE_TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(GROUP_KEY_UPDATE_TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(IE_IN_4WAY_DIFFERS)
    JL_WIFI_REASON_CASE_RETURN(GROUP_CIPHER_INVALID)
    JL_WIFI_REASON_CASE_RETURN(PAIRWISE_CIPHER_INVALID)
    JL_WIFI_REASON_CASE_RETURN(AKMP_INVALID)
    JL_WIFI_REASON_CASE_RETURN(UNSUPP_RSN_IE_VERSION)
    JL_WIFI_REASON_CASE_RETURN(INVALID_RSN_IE_CAP)
    JL_WIFI_REASON_CASE_RETURN(802_1X_AUTH_FAILED)
    JL_WIFI_REASON_CASE_RETURN(CIPHER_SUITE_REJECTED)
    JL_WIFI_REASON_CASE_RETURN(TDLS_PEER_UNREACHABLE)
    JL_WIFI_REASON_CASE_RETURN(TDLS_UNSPECIFIED)
    JL_WIFI_REASON_CASE_RETURN(SSP_REQUESTED_DISASSOC)
    JL_WIFI_REASON_CASE_RETURN(NO_SSP_ROAMING_AGREEMENT)
    JL_WIFI_REASON_CASE_RETURN(BAD_CIPHER_OR_AKM)
    JL_WIFI_REASON_CASE_RETURN(NOT_AUTHORIZED_THIS_LOCATION)
    JL_WIFI_REASON_CASE_RETURN(SERVICE_CHANGE_PERCLUDES_TS)
    JL_WIFI_REASON_CASE_RETURN(UNSPECIFIED_QOS)
    JL_WIFI_REASON_CASE_RETURN(NOT_ENOUGH_BANDWIDTH)
    JL_WIFI_REASON_CASE_RETURN(MISSING_ACKS)
    JL_WIFI_REASON_CASE_RETURN(EXCEEDED_TXOP)
    JL_WIFI_REASON_CASE_RETURN(STA_LEAVING)
    JL_WIFI_REASON_CASE_RETURN(END_BA)
    JL_WIFI_REASON_CASE_RETURN(UNKNOWN_BA)
    JL_WIFI_REASON_CASE_RETURN(TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(PEER_INITIATED)
    JL_WIFI_REASON_CASE_RETURN(AP_INITIATED)
    JL_WIFI_REASON_CASE_RETURN(INVALID_FT_ACTION_FRAME_COUNT)
    JL_WIFI_REASON_CASE_RETURN(INVALID_PMKID)
    JL_WIFI_REASON_CASE_RETURN(INVALID_MDE)
    JL_WIFI_REASON_CASE_RETURN(INVALID_FTE)
    JL_WIFI_REASON_CASE_RETURN(TRANSMISSION_LINK_ESTABLISH_FAILED)
    JL_WIFI_REASON_CASE_RETURN(ALTERATIVE_CHANNEL_OCCUPIED)
    JL_WIFI_REASON_CASE_RETURN(BEACON_TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(NO_AP_FOUND)
    JL_WIFI_REASON_CASE_RETURN(AUTH_FAIL)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_FAIL)
    JL_WIFI_REASON_CASE_RETURN(HANDSHAKE_TIMEOUT)
    JL_WIFI_REASON_CASE_RETURN(CONNECTION_FAIL)
    JL_WIFI_REASON_CASE_RETURN(AP_TSF_RESET)
    JL_WIFI_REASON_CASE_RETURN(ROAMING)
    JL_WIFI_REASON_CASE_RETURN(ASSOC_COMEBACK_TIME_TOO_LONG)
    JL_WIFI_REASON_CASE_RETURN(SA_QUERY_TIMEOUT)
#undef JL_WIFI_REASON_CASE_RETURN
  }
  std::ostringstream s;
  s << "UNKNOWN(" << static_cast<int>(reason) << ")";
  return s.str();
}
}  // namespace

std::string Esp32WiFiNetwork::GetStatusStr() {
  struct in_addr localAddress = {};
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    memcpy(&localAddress, &localAddress_, sizeof(localAddress));
  }
  struct in_addr emptyAddress = {INADDR_ANY};
  if (memcmp(&emptyAddress, &localAddress, sizeof(localAddress)) != 0) {
    const OptionalMicroseconds lastRcv = GetLastReceiveTime();
    char addressString[INET_ADDRSTRLEN + 1] = {};
    if (inet_ntop(AF_INET, &localAddress, addressString, sizeof(addressString) - 1) == nullptr) {
      jll_fatal("Esp32WiFiNetwork printing local address failed with error %d: %s", errno, strerror(errno));
    }
    char statStr[100] = {};
    snprintf(statStr, sizeof(statStr) - 1, "%s %s - %lldms", WiFiSsid(), addressString,
             lastRcv ? MsSinceForLogs(*lastRcv) : -1);
    return std::string(statStr);
  } else {
    return "disconnected";
  }
}

void Esp32WiFiNetwork::SetMessageToSend(const ProtocolMessage& messageToSend) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void Esp32WiFiNetwork::DisableSending() {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

void Esp32WiFiNetwork::TriggerSendAsap() {}

std::list<ProtocolMessage> Esp32WiFiNetwork::GetReceivedMessagesImpl() {
  std::list<ProtocolMessage> results;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    results = std::move(receivedMessages_);
    receivedMessages_.clear();
  }
  return results;
}

void Esp32WiFiNetwork::CreateSocket() {
  CloseSocket();
  socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_ < 0) { jll_fatal("Esp32WiFiNetwork socket() failed with error %d: %s", errno, strerror(errno)); }
  int one = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
    jll_fatal("Esp32WiFiNetwork SO_REUSEADDR failed with error %d: %s", errno, strerror(errno));
  }
  struct sockaddr_in sin = {
      .sin_len = sizeof(struct sockaddr_in),
      .sin_family = AF_INET,
      .sin_port = htons(DefaultUdpPort()),
      .sin_addr = {htonl(INADDR_ANY)},
      .sin_zero = {},
  };
  if (bind(socket_, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) != 0) {
    jll_fatal("Esp32WiFiNetwork bind() failed with error %d: %s", errno, strerror(errno));
  }
  uint8_t ttl = 1;
  if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) != 0) {
    jll_fatal("Esp32WiFiNetwork IP_MULTICAST_TTL failed with error %d: %s", errno, strerror(errno));
  }

  {
    const std::lock_guard<std::mutex> lock(mutex_);
    struct ip_mreq multicastGroup = {
        .imr_multiaddr = multicastAddress_,
        .imr_interface = localAddress_,
    };
    if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multicastGroup, sizeof(multicastGroup)) < 0) {
      jll_fatal("Esp32WiFiNetwork joining multicast failed with error %d: %s", errno, strerror(errno));
    }
    if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF, &localAddress_, sizeof(localAddress_)) < 0) {
      jll_fatal("Esp32WiFiNetwork IP_MULTICAST_IF failed with error %d: %s", errno, strerror(errno));
    }
  }

  // Disable receiving our own multicast traffic.
  uint8_t zero = 0;
  if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP, &zero, sizeof(zero)) < 0) {
    jll_fatal("Esp32WiFiNetwork disabling multicast loopack failed with error %d: %s", errno, strerror(errno));
  }

  int flags = fcntl(socket_, F_GETFL) | O_NONBLOCK;
  if (fcntl(socket_, F_SETFL, flags) < 0) {
    jll_fatal("Esp32WiFiNetwork setting nonblocking failed with error %d: %s", errno, strerror(errno));
  }

  jll_info("Esp32WiFiNetwork created socket");
  // Notify our task that the socket is ready.
  Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kSocketReady);
  xQueueOverwrite(eventQueue_, &networkEvent);
}

void Esp32WiFiNetwork::CloseSocket() {
  if (socket_ < 0) { return; }
  jll_info("Esp32WiFiNetwork closing socket");
  close(socket_);
  socket_ = -1;
}

// static
void Esp32WiFiNetwork::EventHandler(void* eventHandlerArg, esp_event_base_t eventBase, int32_t eventId,
                                    void* eventData) {
  Esp32WiFiNetwork* wifiNetwork = reinterpret_cast<Esp32WiFiNetwork*>(eventHandlerArg);
  wifiNetwork->HandleEvent(eventBase, eventId, eventData);
}

void Esp32WiFiNetwork::HandleEvent(esp_event_base_t eventBase, int32_t eventId, void* eventData) {
  if (eventBase == WIFI_EVENT) {
    if (eventId == WIFI_EVENT_STA_START) {
      jll_info("Esp32WiFiNetwork STA started");
      Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kStationStarted);
      xQueueOverwrite(eventQueue_, &networkEvent);
    } else if (eventId == WIFI_EVENT_STA_CONNECTED) {
      jll_info("Esp32WiFiNetwork STA connected");
    } else if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
      wifi_event_sta_disconnected_t* event = reinterpret_cast<wifi_event_sta_disconnected_t*>(eventData);
      jll_info("Esp32WiFiNetwork STA disconnected: %s", WiFiReasonToString(event->reason).c_str());
      Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kStationDisconnected);
      xQueueOverwrite(eventQueue_, &networkEvent);
    }
  } else if (eventBase == IP_EVENT) {
    if (eventId == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = reinterpret_cast<ip_event_got_ip_t*>(eventData);
      jll_info("Esp32WiFiNetwork got IP: " IPSTR, IP2STR(&event->ip_info.ip));
      Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kGotIp);
      memcpy(&networkEvent.data.address, &event->ip_info.ip, sizeof(networkEvent.data.address));
      xQueueOverwrite(eventQueue_, &networkEvent);
    } else if (eventId == IP_EVENT_STA_LOST_IP) {
      jll_info("Esp32WiFiNetwork lost IP");
      Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kLostIp);
      xQueueOverwrite(eventQueue_, &networkEvent);
    } else if (eventId == IP_EVENT_GOT_IP6) {
      jll_info("Esp32WiFiNetwork got IPv6");
    }
  }
}

// static
void Esp32WiFiNetwork::TaskFunction(void* parameters) {
  Esp32WiFiNetwork* wifiNetwork = reinterpret_cast<Esp32WiFiNetwork*>(parameters);
  while (true) { wifiNetwork->RunTask(); }
}

void Esp32WiFiNetwork::HandleNetworkEvent(const Esp32WiFiNetworkEvent& networkEvent) {
  switch (networkEvent.type) {
    case Esp32WiFiNetworkEvent::Type::kReserved: jll_fatal("Unexpected Esp32WiFiNetworkEvent::Type::kReserved"); break;
    case Esp32WiFiNetworkEvent::Type::kStationStarted:
      jll_info("Esp32WiFiNetwork queue station started - connecting");
      esp_wifi_connect();
      break;
    case Esp32WiFiNetworkEvent::Type::kStationDisconnected:
      reconnectCount_++;
      if (reconnectCount_ < kNumReconnectsBeforeDelay) {
        jll_info("Esp32WiFiNetwork queue station disconnected (count %lld) - reconnecting immediately",
                 static_cast<long long>(reconnectCount_));
        esp_wifi_connect();
      } else {
        jll_info("Esp32WiFiNetwork queue station disconnected (count %lld)", static_cast<long long>(reconnectCount_));
        shouldArmQueueReconnectionTimeout_ = true;
      }
      break;
    case Esp32WiFiNetworkEvent::Type::kGotIp:
      jll_info("Esp32WiFiNetwork queue got IP");
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        memcpy(&localAddress_, &networkEvent.data.address, sizeof(localAddress_));
      }
      reconnectCount_ = 0;
      CreateSocket();
      break;
    case Esp32WiFiNetworkEvent::Type::kLostIp:
      jll_info("Esp32WiFiNetwork queue lost IP");
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        memset(&localAddress_, 0, sizeof(localAddress_));
      }
      CloseSocket();
      break;
    case Esp32WiFiNetworkEvent::Type::kSocketReady: jll_info("Esp32WiFiNetwork queue SocketReady"); break;
  }
}

void Esp32WiFiNetwork::RunTask() {
  Esp32WiFiNetworkEvent networkEvent;
  while (xQueueReceive(eventQueue_, &networkEvent, /*xTicksToWait=*/0)) { HandleNetworkEvent(networkEvent); }
  if (socket_ < 0) {
    // Wait until socket is created, or a Wi-Fi reconnection timeout.
    TickType_t queueDelay = portMAX_DELAY;
    if (shouldArmQueueReconnectionTimeout_ && reconnectCount_ >= kNumReconnectsBeforeDelay) {
      shouldArmQueueReconnectionTimeout_ = false;
      // Backoff exponentially from 1s to 32s.
      queueDelay = 1 << std::min<uint32_t>(reconnectCount_ - kNumReconnectsBeforeDelay, 5);
      jll_info("Esp32WiFiNetwork waiting for queue event with %llds timeout", static_cast<long long>(queueDelay));
      queueDelay *= 1000 / portTICK_PERIOD_MS;
    } else {
      jll_info("Esp32WiFiNetwork waiting for queue event forever");
    }
    BaseType_t queueResult = xQueueReceive(eventQueue_, &networkEvent, queueDelay);
    if (queueResult == pdTRUE) {
      HandleNetworkEvent(networkEvent);
    } else {
      jll_info("Esp32WiFiNetwork timed out waiting for queue event - reconnecting");
      esp_wifi_connect();
    }
    return;  // Restart loop.
  }
  ProtocolMessage messageToSend;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    messageToSend = messageToSend_;
  }
  Microseconds currentTime = TimeMicros();
  if (hasDataToSend_ && (!lastSendTime_ || currentTime - *lastSendTime_ >= kSendInterval ||
                         messageToSend.currentPattern != lastSentPattern_)) {
    lastSendTime_ = currentTime;
    lastSentPattern_ = messageToSend.currentPattern;
    if (!WriteUdpPayload(messageToSend, udpPayload_, kReceiveBufferLength)) {
      jll_fatal("Esp32WiFiNetwork unexpected payload length issue");
    }
    struct sockaddr_in sin = {
        .sin_len = sizeof(struct sockaddr_in),
        .sin_family = AF_INET,
        .sin_port = htons(DefaultUdpPort()),
        .sin_addr = multicastAddress_,
        .sin_zero = {},
    };
    ssize_t writeRes =
        sendto(socket_, udpPayload_, kReceiveBufferLength, /*flags=*/0, reinterpret_cast<sockaddr*>(&sin), sizeof(sin));
    (void)writeRes;
  }

  // Now receive.
  sockaddr_in sin = {};
  socklen_t sinLength = sizeof(sin);
  ssize_t n =
      recvfrom(socket_, udpPayload_, kReceiveBufferLength, /*flags=*/0, reinterpret_cast<sockaddr*>(&sin), &sinLength);
  if (n < 0) {
    const int errorCode = errno;
    static_assert(EWOULDBLOCK == EAGAIN, "need to handle these separately");
    if (errorCode == EWOULDBLOCK) {
      struct pollfd pollFd = {
          .fd = socket_,
          .events = POLLIN,
          .revents = 0,
      };
      int pollTimeoutMs = static_cast<int>(kSendInterval / kMicrosecondsPerMillisecond);
      if (lastSendTime_) {
        const Microseconds timeSinceLastSendMs =
            static_cast<int>((TimeMicros() - *lastSendTime_) / kMicrosecondsPerMillisecond);
        if (timeSinceLastSendMs <= pollTimeoutMs) { pollTimeoutMs -= timeSinceLastSendMs; }
      }
      int pollRes = poll(&pollFd, 1, pollTimeoutMs);
      if (pollRes > 0) {  // Data available.
        // Do nothing, just restart loop to read.
      } else if (pollRes == 0) {  // Timed out.
                                  // Do nothing, just restart loop to write.
      } else {                    // Error.
        jll_error("Esp32WiFiNetwork poll failed with error %d: %s", errno, strerror(errno));
        CreateSocket();
      }
      return;  // Restart loop.
    }
    jll_error("Esp32WiFiNetwork recvfrom failed with error %d: %s", errno, strerror(errno));
    CreateSocket();
    return;
  }
  std::ostringstream s;
  char addressString[INET_ADDRSTRLEN] = {};
  if (inet_ntop(AF_INET, &(sin.sin_addr), addressString, sizeof(addressString)) == nullptr) {
    jll_fatal("Esp32WiFiNetwork printing receive address failed with error %d: %s", errno, strerror(errno));
  }
  s << " (from " << addressString << ":" << ntohs(sin.sin_port) << ")";
  std::string receiptDetails = s.str();
  ProtocolMessage receivedMessage;
  if (ParseUdpPayload(udpPayload_, n, receiptDetails, &receivedMessage)) {
    lastReceiveTime_.store(TimeMicros(), std::memory_order_relaxed);
    const std::lock_guard<std::mutex> lock(mutex_);
    receivedMessages_.push_back(receivedMessage);
  }
}

NetworkDeviceId Esp32WiFiNetwork::InitWiFiStackAndQueryLocalDeviceId() {
  InitializeNetStack();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifiInitConfig = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifiInitConfig));

  esp_event_handler_instance_t wifiInstance;
  esp_event_handler_instance_t ipInstance;
  // Register event handlers on the default event loop. That runs in task "sys_evt" on core 0 (and is not
  // configurable).
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &wifiInstance));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &ipInstance));

  wifi_config_t wifiConfig;
  memset(&wifiConfig, 0, sizeof(wifiConfig));
  wifiConfig.sta = {};
  strncpy(reinterpret_cast<char*>(wifiConfig.sta.ssid), WiFiSsid(), sizeof(wifiConfig.sta.ssid) - 1);
  strncpy(reinterpret_cast<char*>(wifiConfig.sta.password), WiFiPassword(), sizeof(wifiConfig.sta.password) - 1);
  // TODO add support for IPv4 link-local addressing in the absence of DHCP. It looks like ESP-IDF supports it via
  // CONFIG_LWIP_AUTOIP but that doesn't seem to exist in our sdkconfig. I think that's because ESP-IDF disables it by
  // default, and the Arduino Core doesn't override that, so we'll need a custom ESP-IDF sdkconfig to enable it.
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifiConfig));
  ESP_ERROR_CHECK(esp_wifi_start());

  uint8_t macAddress[6];
  ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, macAddress));
  return NetworkDeviceId(macAddress);
}

Esp32WiFiNetwork::Esp32WiFiNetwork()
    : eventQueue_(xQueueCreate(/*numQueueItems=*/1, /*queueItemSize=*/sizeof(Esp32WiFiNetworkEvent))) {
  if (eventQueue_ == nullptr) { jll_fatal("Failed to create Esp32WiFiNetwork queue"); }
  udpPayload_ = reinterpret_cast<uint8_t*>(malloc(kReceiveBufferLength));
  if (udpPayload_ == nullptr) {
    jll_fatal("Esp32WiFiNetwork failed to allocate receive buffer of length %zu", kReceiveBufferLength);
  }

  if (inet_pton(AF_INET, DefaultMulticastAddress(), &multicastAddress_) != 1) {
    jll_fatal("Esp32WiFiNetwork failed to parse multicast address");
  }

  // This task needs to be pinned to core 0 since that's where the system event handler runs (see above).
  if (xTaskCreatePinnedToCore(TaskFunction, "JL_WiFi", configMINIMAL_STACK_SIZE + 2000,
                              /*parameters=*/this, kHighestTaskPriority, &taskHandle_, /*coreID=*/0) != pdPASS) {
    jll_fatal("Failed to create Esp32WiFiNetwork task");
  }

  jll_info("Esp32WiFiNetwork initialized Wi-Fi STA with MAC address " DEVICE_ID_FMT, DEVICE_ID_HEX(localDeviceId_));
}

// static
Esp32WiFiNetwork* Esp32WiFiNetwork::Get() {
  static Esp32WiFiNetwork sSingleton;
  return &sSingleton;
}

Esp32WiFiNetwork::~Esp32WiFiNetwork() {
  jll_fatal("Destructing Esp32WiFiNetwork is not currently supported");
  // This destruction is unsafe since a race condition could cause the event handler to fire after the destructor is
  // called. If we ever have a need to destroy this, we'll need to make this safe first.
  // CloseSocket();
}

}  // namespace jazzlights

#endif  // JL_WIFI
