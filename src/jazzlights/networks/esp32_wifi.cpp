#include "jazzlights/networks/esp32_wifi.h"

#if JL_WIFI

#include <esp_event.h>
#include <esp_wifi.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <string.h>

#include <sstream>

#include "jazzlights/esp32_shared.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
constexpr size_t kReceiveBufferLength = 1500;
constexpr Milliseconds kSendInterval = 100;
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

NetworkStatus Esp32WiFiNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  if (status == INITIALIZING || status == CONNECTING) {
    return CONNECTED;
  } else if (status == DISCONNECTING) {
    return DISCONNECTED;
  } else {
    return status;
  }
}

std::string Esp32WiFiNetwork::getStatusStr(Milliseconds currentTime) {
  struct in_addr localAddress = {};
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    memcpy(&localAddress, &localAddress_, sizeof(localAddress));
  }
  struct in_addr emptyAddress = {INADDR_ANY};
  if (memcmp(&emptyAddress, &localAddress, sizeof(localAddress)) != 0) {
    const Milliseconds lastRcv = getLastReceiveTime();
    char addressString[INET_ADDRSTRLEN + 1] = {};
    if (inet_ntop(AF_INET, &localAddress, addressString, sizeof(addressString) - 1) == nullptr) {
      jll_fatal("Esp32WiFiNetwork printing local address failed with error %d: %s", errno, strerror(errno));
    }
    char statStr[100] = {};
    snprintf(statStr, sizeof(statStr) - 1, "%s %s - %ums", JL_WIFI_SSID, addressString,
             (lastRcv >= 0 ? currentTime - getLastReceiveTime() : -1));
    return std::string(statStr);
  } else {
    return "disconnected";
  }
}

void Esp32WiFiNetwork::setMessageToSend(const NetworkMessage& messageToSend, Milliseconds /*currentTime*/) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void Esp32WiFiNetwork::disableSending(Milliseconds /*currentTime*/) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

void Esp32WiFiNetwork::triggerSendAsap(Milliseconds /*currentTime*/) {}

std::list<NetworkMessage> Esp32WiFiNetwork::getReceivedMessagesImpl(Milliseconds /*currentTime*/) {
  std::list<NetworkMessage> results;
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

  jll_info("%u Esp32WiFiNetwork created socket", timeMillis());
  // Notify our task that the socket is ready.
  Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kSocketReady);
  xQueueOverwrite(eventQueue_, &networkEvent);
}

void Esp32WiFiNetwork::CloseSocket() {
  if (socket_ < 0) { return; }
  jll_info("%u Esp32WiFiNetwork closing socket", timeMillis());
  close(socket_);
  socket_ = -1;
}

// static
void Esp32WiFiNetwork::EventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                    void* event_data) {
  Esp32WiFiNetwork* wifiNetwork = reinterpret_cast<Esp32WiFiNetwork*>(event_handler_arg);
  wifiNetwork->HandleEvent(event_base, event_id, event_data);
}

void Esp32WiFiNetwork::HandleEvent(esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      jll_info("%u Esp32WiFiNetwork STA started", timeMillis());
      Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kStationStarted);
      xQueueOverwrite(eventQueue_, &networkEvent);
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
      jll_info("%u Esp32WiFiNetwork STA connected", timeMillis());
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      wifi_event_sta_disconnected_t* event = reinterpret_cast<wifi_event_sta_disconnected_t*>(event_data);
      jll_info("%u Esp32WiFiNetwork STA disconnected: %s", timeMillis(), WiFiReasonToString(event->reason).c_str());
      Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kStationDisconnected);
      xQueueOverwrite(eventQueue_, &networkEvent);
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = reinterpret_cast<ip_event_got_ip_t*>(event_data);
      jll_info("%u Esp32WiFiNetwork got IP: " IPSTR, timeMillis(), IP2STR(&event->ip_info.ip));
      Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kGotIp);
      memcpy(&networkEvent.data.address, &event->ip_info.ip, sizeof(networkEvent.data.address));
      xQueueOverwrite(eventQueue_, &networkEvent);
    } else if (event_id == IP_EVENT_STA_LOST_IP) {
      jll_info("%u Esp32WiFiNetwork lost IP", timeMillis());
      Esp32WiFiNetworkEvent networkEvent(Esp32WiFiNetworkEvent::Type::kLostIp);
      xQueueOverwrite(eventQueue_, &networkEvent);
    } else if (event_id == IP_EVENT_GOT_IP6) {
      jll_info("%u Esp32WiFiNetwork got IPv6", timeMillis());
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
      jll_info("%u Esp32WiFiNetwork queue station started - connecting", timeMillis());
      esp_wifi_connect();
      break;
    case Esp32WiFiNetworkEvent::Type::kStationDisconnected:
      reconnectCount_++;
      if (reconnectCount_ < kNumReconnectsBeforeDelay) {
        jll_info("%u Esp32WiFiNetwork queue station disconnected (count %u) - reconnecting immediately", timeMillis(),
                 reconnectCount_);
        esp_wifi_connect();
      } else {
        jll_info("%u Esp32WiFiNetwork queue station disconnected (count %u)", timeMillis(), reconnectCount_);
        shouldArmQueueReconnectionTimeout_ = true;
      }
      break;
    case Esp32WiFiNetworkEvent::Type::kGotIp:
      jll_info("%u Esp32WiFiNetwork queue got IP", timeMillis());
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        memcpy(&localAddress_, &networkEvent.data.address, sizeof(localAddress_));
      }
      reconnectCount_ = 0;
      CreateSocket();
      break;
    case Esp32WiFiNetworkEvent::Type::kLostIp:
      jll_info("%u Esp32WiFiNetwork queue lost IP", timeMillis());
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        memset(&localAddress_, 0, sizeof(localAddress_));
      }
      CloseSocket();
      break;
    case Esp32WiFiNetworkEvent::Type::kSocketReady:
      jll_info("%u Esp32WiFiNetwork queue SocketReady", timeMillis());
      break;
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
      jll_info("%u Esp32WiFiNetwork waiting for queue event with %us timeout", timeMillis(), queueDelay);
      queueDelay *= 1000 / portTICK_PERIOD_MS;
    } else {
      jll_info("%u Esp32WiFiNetwork waiting for queue event forever", timeMillis());
    }
    BaseType_t queueResult = xQueueReceive(eventQueue_, &networkEvent, queueDelay);
    if (queueResult == pdTRUE) {
      HandleNetworkEvent(networkEvent);
    } else {
      jll_info("%u Esp32WiFiNetwork timed out waiting for queue event - reconnecting", timeMillis());
      esp_wifi_connect();
    }
    return;  // Restart loop.
  }
  NetworkMessage messageToSend;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    messageToSend = messageToSend_;
  }
  Milliseconds currentTime = timeMillis();
  if (hasDataToSend_ && (lastSendTime_ < 1 || currentTime - lastSendTime_ >= kSendInterval ||
                         messageToSend.currentPattern != lastSentPattern_)) {
    lastSendTime_ = currentTime;
    lastSentPattern_ = messageToSend.currentPattern;
    if (!WriteUdpPayload(messageToSend, udpPayload_, kReceiveBufferLength, currentTime)) {
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
      Milliseconds timeout = kSendInterval - (timeMillis() - lastSendTime_);
      int pollRes = poll(&pollFd, 1, timeout);
      if (pollRes > 0) {  // Data available.
        // Do nothing, just restart loop to read.
      } else if (pollRes == 0) {  // Timed out.
                                  // Do nothing, just restart loop to write.
      } else {                    // Error.
        jll_error("%u Esp32WiFiNetwork poll failed with error %d: %s", timeMillis(), errno, strerror(errno));
        CreateSocket();
      }
      return;  // Restart loop.
    }
    jll_error("%u Esp32WiFiNetwork recvfrom failed with error %d: %s", timeMillis(), errno, strerror(errno));
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
  NetworkMessage receivedMessage;
  if (ParseUdpPayload(udpPayload_, n, receiptDetails, currentTime, &receivedMessage)) {
    lastReceiveTime_.store(timeMillis(), std::memory_order_relaxed);
    const std::lock_guard<std::mutex> lock(mutex_);
    receivedMessages_.push_back(receivedMessage);
  }
}

Esp32WiFiNetwork::Esp32WiFiNetwork()
    : eventQueue_(xQueueCreate(/*num_queue_items=*/1, /*queue_item_size=*/sizeof(Esp32WiFiNetworkEvent))) {
  if (eventQueue_ == nullptr) { jll_fatal("Failed to create Esp32WiFiNetwork queue"); }
  udpPayload_ = reinterpret_cast<uint8_t*>(malloc(kReceiveBufferLength));
  if (udpPayload_ == nullptr) {
    jll_fatal("Esp32WiFiNetwork failed to allocate receive buffer of length %zu", kReceiveBufferLength);
  }
  InitializeNetStack();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

  esp_event_handler_instance_t wifiInstance;
  esp_event_handler_instance_t ipInstance;
  // Register event handlers on the default event loop. That runs in task "sys_evt" on core 0 (and is not
  // configurable).
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &wifiInstance));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &ipInstance));

  wifi_config_t wifi_config;
  memset(&wifi_config, 0, sizeof(wifi_config));
  wifi_config.sta = {};
  strncpy(reinterpret_cast<char*>(wifi_config.sta.ssid), JL_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
  strncpy(reinterpret_cast<char*>(wifi_config.sta.password), JL_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
  // TODO add support for IPv4 link-local addressing in the absence of DHCP. It looks like ESP-IDF supports it via
  // CONFIG_LWIP_AUTOIP but that doesn't seem to exist in our sdkconfig. I think that's because ESP-IDF disables it by
  // default, and the Arduino Core doesn't override that, so we'll need a custom ESP-IDF sdkconfig to enable it.
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  uint8_t macAddress[6];
  ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, macAddress));
  localDeviceId_ = NetworkDeviceId(macAddress);

  if (inet_pton(AF_INET, DefaultMulticastAddress(), &multicastAddress_) != 1) {
    jll_fatal("Esp32WiFiNetwork failed to parse multicast address");
  }

  // This task needs to be pinned to core 0 since that's where the system event handler runs (see above).
  if (xTaskCreatePinnedToCore(TaskFunction, "JL_WiFi", configMINIMAL_STACK_SIZE + 2000,
                              /*parameters=*/this,
                              /*priority=*/30, &taskHandle_, /*coreID=*/0) != pdPASS) {
    jll_fatal("Failed to create Esp32WiFiNetwork task");
  }

  jll_info("%u Esp32WiFiNetwork initialized Wi-Fi STA with MAC address %s", timeMillis(),
           localDeviceId_.toString().c_str());
}

// static
Esp32WiFiNetwork* Esp32WiFiNetwork::get() {
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
