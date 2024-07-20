#include "jazzlights/networks/esp32_wifi.h"

#ifdef ESP32
#if JL_ESP32_WIFI

#include <esp_wifi.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <string.h>

#include <sstream>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
constexpr size_t kReceiveBufferLength = 1500;
constexpr Milliseconds kSendInterval = 100;
// Index 0 is normally reserved by FreeRTOS for stream and message buffers. However, the default precompiled FreeRTOS
// kernel for arduino/esp-idf only allows a single notification, so we use index 0 here. We don't use stream or message
// buffers on this specific task so we should be fine.
constexpr UBaseType_t kWiFiNotificationIndex = 0;
static_assert(kWiFiNotificationIndex < configTASK_NOTIFICATION_ARRAY_ENTRIES, "index too high");
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

std::string Esp32WiFiNetwork::getStatusStr(Milliseconds currentTime) const { return "silly"; }

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
  (void)xTaskGenericNotify(taskHandle_, kWiFiNotificationIndex,
                           /*notification_value=*/0, eNoAction, /*previousNotificationValue=*/nullptr);
}

void Esp32WiFiNetwork::CloseSocket() {
  if (socket_ < 0) { return; }
  jll_info("Esp32WiFiNetwork closing socket");
  close(socket_);
  socket_ = -1;
}

// static
void Esp32WiFiNetwork::EventHandler(void* /*event_handler_arg*/, esp_event_base_t event_base, int32_t event_id,
                                    void* event_data) {
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      jll_info("Esp32WiFiNetwork STA started - connecting");
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
      jll_info("Esp32WiFiNetwork STA connected");
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      jll_info("Esp32WiFiNetwork STA disconnected - reconnecting");
      // TODO Consider adding some kind of delay (10s?) when reconnecting fails repeatedly to avoid draining battery.
    }
    if (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED) { esp_wifi_connect(); }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
      jll_info("Esp32WiFiNetwork got IP: " IPSTR, IP2STR(&event->ip_info.ip));
      memcpy(&get()->localAddress_, &event->ip_info.ip, sizeof(get()->localAddress_));
      get()->CreateSocket();
    } else if (event_id == IP_EVENT_STA_LOST_IP) {
      jll_info("Esp32WiFiNetwork lost IP");
      get()->CloseSocket();
    } else if (event_id == IP_EVENT_GOT_IP6) {
      jll_info("Esp32WiFiNetwork got IPv6");
    }
  }
}

// static
void Esp32WiFiNetwork::TaskFunction(void* parameters) {
  Esp32WiFiNetwork* network = reinterpret_cast<Esp32WiFiNetwork*>(parameters);
  while (true) { network->RunTask(); }
}

void Esp32WiFiNetwork::RunTask() {
  if (socket_ < 0) {
    // Wait until socket is created.
    jll_info("Esp32WiFiNetwork waiting for socket");
    (void)ulTaskGenericNotifyTake(kWiFiNotificationIndex, pdTRUE, portMAX_DELAY);
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
      jll_fatal("%s unexpected payload length issue", networkName());
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
  NetworkMessage receivedMessage;
  if (ParseUdpPayload(udpPayload_, n, receiptDetails, currentTime, &receivedMessage)) {
    lastReceiveTime_.store(timeMillis(), std::memory_order_relaxed);
    const std::lock_guard<std::mutex> lock(mutex_);
    receivedMessages_.push_back(receivedMessage);
  }
}

Esp32WiFiNetwork::Esp32WiFiNetwork() {
  udpPayload_ = reinterpret_cast<uint8_t*>(malloc(kReceiveBufferLength));
  if (udpPayload_ == nullptr) {
    jll_fatal("Esp32WiFiNetwork failed to allocate receive buffer of length %zu", kReceiveBufferLength);
  }
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &EventHandler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(
      esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandler, NULL, &instance_got_ip));

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

  // TODO figure out correct stack size
  if (xTaskCreatePinnedToCore(TaskFunction, "JL_WiFi", configMINIMAL_STACK_SIZE + 4000,
                              /*parameters=*/this,
                              /*priority=*/30, &taskHandle_, /*coreID=*/0) != pdPASS) {
    jll_fatal("Failed to create Esp32WiFiNetwork task");
  }

  jll_info("Esp32WiFiNetwork initialized Wi-Fi STA with MAC address %s", localDeviceId_.toString().c_str());
}

// static
Esp32WiFiNetwork* Esp32WiFiNetwork::get() {
  static Esp32WiFiNetwork sSingleton;
  return &sSingleton;
}

Esp32WiFiNetwork::~Esp32WiFiNetwork() { CloseSocket(); }

}  // namespace jazzlights

#endif  // JL_ESP32_WIFI
#endif  // ESP32
