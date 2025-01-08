#include "jazzlights/network/esp32_ethernet.h"

#if JL_ESP32_ETHERNET

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <esp_eth.h>
#include <esp_event.h>
#include <esp_idf_version.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <string.h>

#include <sstream>

#include "jazzlights/config.h"
#include "jazzlights/esp32_shared.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
constexpr size_t kReceiveBufferLength = 1500;
constexpr Milliseconds kSendInterval = 100;
constexpr uint32_t kNumReconnectsBeforeDelay = 10;
#if JL_CORE2AWS_ETHERNET
constexpr int kEthernetPinSCK = 18;
constexpr int kEthernetPinCS = 26;
constexpr int kEthernetPinMISO = 38;
constexpr int kEthernetPinMOSI = 23;
constexpr int kEthernetPinReset = 19;
constexpr int kEthernetPinInterrupt = 34;
#elif JL_IS_CONTROLLER(ATOM_MATRIX) || JL_IS_CONTROLLER(ATOM_LITE)
constexpr int kEthernetPinSCK = 22;
constexpr int kEthernetPinCS = 19;
constexpr int kEthernetPinMISO = 23;
constexpr int kEthernetPinMOSI = 33;
constexpr int kEthernetPinReset = -1;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
constexpr int kEthernetPinInterrupt = -1;
#else
constexpr int kEthernetPinInterrupt = 21;  // Fake because -1 isn't supported in ESP-IDF v4.
#endif
#elif JL_IS_CONTROLLER(ATOM_S3)
constexpr int kEthernetPinSCK = 5;
constexpr int kEthernetPinCS = 6;
constexpr int kEthernetPinMISO = 7;
constexpr int kEthernetPinMOSI = 8;
constexpr int kEthernetPinReset = -1;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
constexpr int kEthernetPinInterrupt = -1;
#else
constexpr int kEthernetPinInterrupt = 21;  // Fake because -1 isn't supported in ESP-IDF v4.
#endif
#else
#error "Unexpected controller"
#endif
}  // namespace

NetworkStatus Esp32EthernetNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  if (status == INITIALIZING || status == CONNECTING) {
    return CONNECTED;
  } else if (status == DISCONNECTING) {
    return DISCONNECTED;
  } else {
    return status;
  }
}

std::string Esp32EthernetNetwork::getStatusStr(Milliseconds currentTime) {
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
      jll_fatal("Esp32EthernetNetwork printing local address failed with error %d: %s", errno, strerror(errno));
    }
    char statStr[100] = {};
    snprintf(statStr, sizeof(statStr) - 1, "%s - %ums", addressString,
             (lastRcv >= 0 ? currentTime - getLastReceiveTime() : -1));
    return std::string(statStr);
  } else {
    return "disconnected";
  }
}

void Esp32EthernetNetwork::setMessageToSend(const NetworkMessage& messageToSend, Milliseconds /*currentTime*/) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void Esp32EthernetNetwork::disableSending(Milliseconds /*currentTime*/) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

void Esp32EthernetNetwork::triggerSendAsap(Milliseconds /*currentTime*/) {}

std::list<NetworkMessage> Esp32EthernetNetwork::getReceivedMessagesImpl(Milliseconds /*currentTime*/) {
  std::list<NetworkMessage> results;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    results = std::move(receivedMessages_);
    receivedMessages_.clear();
  }
  return results;
}

void Esp32EthernetNetwork::CreateSocket() {
  CloseSocket();
  socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_ < 0) { jll_fatal("Esp32EthernetNetwork socket() failed with error %d: %s", errno, strerror(errno)); }
  int one = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
    jll_fatal("Esp32EthernetNetwork SO_REUSEADDR failed with error %d: %s", errno, strerror(errno));
  }
  struct sockaddr_in sin = {
      .sin_len = sizeof(struct sockaddr_in),
      .sin_family = AF_INET,
      .sin_port = htons(DefaultUdpPort()),
      .sin_addr = {htonl(INADDR_ANY)},
      .sin_zero = {},
  };
  if (bind(socket_, reinterpret_cast<struct sockaddr*>(&sin), sizeof(sin)) != 0) {
    jll_fatal("Esp32EthernetNetwork bind() failed with error %d: %s", errno, strerror(errno));
  }
  uint8_t ttl = 1;
  if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) != 0) {
    jll_fatal("Esp32EthernetNetwork IP_MULTICAST_TTL failed with error %d: %s", errno, strerror(errno));
  }

  {
    const std::lock_guard<std::mutex> lock(mutex_);
    struct ip_mreq multicastGroup = {
        .imr_multiaddr = multicastAddress_,
        .imr_interface = localAddress_,
    };
    if (setsockopt(socket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &multicastGroup, sizeof(multicastGroup)) < 0) {
      jll_fatal("Esp32EthernetNetwork joining multicast failed with error %d: %s", errno, strerror(errno));
    }
    if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_IF, &localAddress_, sizeof(localAddress_)) < 0) {
      jll_fatal("Esp32EthernetNetwork IP_MULTICAST_IF failed with error %d: %s", errno, strerror(errno));
    }
  }

  // Disable receiving our own multicast traffic.
  uint8_t zero = 0;
  if (setsockopt(socket_, IPPROTO_IP, IP_MULTICAST_LOOP, &zero, sizeof(zero)) < 0) {
    jll_fatal("Esp32EthernetNetwork disabling multicast loopack failed with error %d: %s", errno, strerror(errno));
  }

  int flags = fcntl(socket_, F_GETFL) | O_NONBLOCK;
  if (fcntl(socket_, F_SETFL, flags) < 0) {
    jll_fatal("Esp32EthernetNetwork setting nonblocking failed with error %d: %s", errno, strerror(errno));
  }

  jll_info("%u Esp32EthernetNetwork created socket", timeMillis());
  // Notify our task that the socket is ready.
  Esp32EthernetNetworkEvent networkEvent(Esp32EthernetNetworkEvent::Type::kSocketReady);
  xQueueOverwrite(eventQueue_, &networkEvent);
}

void Esp32EthernetNetwork::CloseSocket() {
  if (socket_ < 0) { return; }
  jll_info("%u Esp32EthernetNetwork closing socket", timeMillis());
  close(socket_);
  socket_ = -1;
}

// static
void Esp32EthernetNetwork::EventHandler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                                        void* event_data) {
  Esp32EthernetNetwork* ethNetwork = reinterpret_cast<Esp32EthernetNetwork*>(event_handler_arg);
  ethNetwork->HandleEvent(event_base, event_id, event_data);
}

void Esp32EthernetNetwork::HandleEvent(esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == ETH_EVENT) {
    if (event_id == ETHERNET_EVENT_CONNECTED) {
      jll_info("Esp32EthernetNetwork got a valid link");
    } else if (event_id == ETHERNET_EVENT_DISCONNECTED) {
      jll_info("Esp32EthernetNetwork lost a valid link");
    } else if (event_id == ETHERNET_EVENT_START) {
      jll_info("Esp32EthernetNetwork driver start");
    } else if (event_id == ETHERNET_EVENT_STOP) {
      jll_info("Esp32EthernetNetwork driver stop");
    } else {
      jll_info("Esp32EthernetNetwork handling ETH_EVENT id %d", static_cast<int>(event_id));
    }
  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_ETH_GOT_IP) {
      ip_event_got_ip_t* event = reinterpret_cast<ip_event_got_ip_t*>(event_data);
      jll_info("%u Esp32EthernetNetwork got IP: " IPSTR, timeMillis(), IP2STR(&event->ip_info.ip));
      Esp32EthernetNetworkEvent networkEvent(Esp32EthernetNetworkEvent::Type::kGotIp);
      memcpy(&networkEvent.data.address, &event->ip_info.ip, sizeof(networkEvent.data.address));
      xQueueOverwrite(eventQueue_, &networkEvent);
    } else if (event_id == IP_EVENT_ETH_LOST_IP) {
      jll_info("%u Esp32EthernetNetwork lost IP", timeMillis());
      Esp32EthernetNetworkEvent networkEvent(Esp32EthernetNetworkEvent::Type::kLostIp);
      xQueueOverwrite(eventQueue_, &networkEvent);
    } else if (event_id == IP_EVENT_GOT_IP6) {
      jll_info("%u Esp32EthernetNetwork got IPv6", timeMillis());
    } else {
      jll_info("Esp32EthernetNetwork handling IP_EVENT id %d", static_cast<int>(event_id));
    }
  } else {
    jll_info("Esp32EthernetNetwork handling unknown event base %p id %d", event_base, (int)event_id);
  }
}

// static
void Esp32EthernetNetwork::TaskFunction(void* parameters) {
  Esp32EthernetNetwork* ethNetwork = reinterpret_cast<Esp32EthernetNetwork*>(parameters);
  while (true) { ethNetwork->RunTask(); }
}

void Esp32EthernetNetwork::HandleNetworkEvent(const Esp32EthernetNetworkEvent& networkEvent) {
  switch (networkEvent.type) {
    case Esp32EthernetNetworkEvent::Type::kReserved:
      jll_fatal("Unexpected Esp32EthernetNetworkEvent::Type::kReserved");
      break;
    case Esp32EthernetNetworkEvent::Type::kGotIp:
      jll_info("%u Esp32EthernetNetwork queue got IP", timeMillis());
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        memcpy(&localAddress_, &networkEvent.data.address, sizeof(localAddress_));
      }
      CreateSocket();
      break;
    case Esp32EthernetNetworkEvent::Type::kLostIp:
      jll_info("%u Esp32EthernetNetwork queue lost IP", timeMillis());
      {
        const std::lock_guard<std::mutex> lock(mutex_);
        memset(&localAddress_, 0, sizeof(localAddress_));
      }
      CloseSocket();
      break;
    case Esp32EthernetNetworkEvent::Type::kSocketReady:
      jll_info("%u Esp32EthernetNetwork queue SocketReady", timeMillis());
      break;
  }
}

void Esp32EthernetNetwork::RunTask() {
  Esp32EthernetNetworkEvent networkEvent;
  while (xQueueReceive(eventQueue_, &networkEvent, /*xTicksToWait=*/0)) { HandleNetworkEvent(networkEvent); }
  if (socket_ < 0) {
    // Wait until socket is created.
    jll_info("%u Esp32EthernetNetwork waiting for queue event forever", timeMillis());
    BaseType_t queueResult = xQueueReceive(eventQueue_, &networkEvent, portMAX_DELAY);
    if (queueResult == pdTRUE) {
      HandleNetworkEvent(networkEvent);
    } else {
      jll_info("%u Esp32EthernetNetwork timed out waiting for queue event", timeMillis());
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
      jll_fatal("Esp32EthernetNetwork unexpected payload length issue");
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
        jll_error("%u Esp32EthernetNetwork poll failed with error %d: %s", timeMillis(), errno, strerror(errno));
        CreateSocket();
      }
      return;  // Restart loop.
    }
    jll_error("%u Esp32EthernetNetwork recvfrom failed with error %d: %s", timeMillis(), errno, strerror(errno));
    CreateSocket();
    return;
  }
  std::ostringstream s;
  char addressString[INET_ADDRSTRLEN] = {};
  if (inet_ntop(AF_INET, &(sin.sin_addr), addressString, sizeof(addressString)) == nullptr) {
    jll_fatal("Esp32EthernetNetwork printing receive address failed with error %d: %s", errno, strerror(errno));
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

Esp32EthernetNetwork::Esp32EthernetNetwork()
    : eventQueue_(xQueueCreate(/*num_queue_items=*/1, /*queue_item_size=*/sizeof(Esp32EthernetNetworkEvent))) {
  if (eventQueue_ == nullptr) { jll_fatal("Failed to create Esp32EthernetNetwork queue"); }
  udpPayload_ = reinterpret_cast<uint8_t*>(malloc(kReceiveBufferLength));
  if (udpPayload_ == nullptr) {
    jll_fatal("Esp32EthernetNetwork failed to allocate receive buffer of length %zu", kReceiveBufferLength);
  }
  // This version was adapted from the esp-idf v5.2.2 example.
  // https://github.com/espressif/esp-idf/blob/v5.2.2/examples/ethernet/basic/main/ethernet_example_main.c
  // https://github.com/espressif/esp-idf/blob/v5.2.2/examples/ethernet/basic/components/ethernet_init/ethernet_init.c

  // Install GPIO ISR handler to be able to service SPI ethernet modules interrupts.
  InstallGpioIsrService();

  spi_host_device_t host_id = SPI3_HOST;

  // Init SPI bus.
  spi_bus_config_t buscfg = {
      .mosi_io_num = kEthernetPinMOSI,
      .miso_io_num = kEthernetPinMISO,
      .sclk_io_num = kEthernetPinSCK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
  };
  esp_err_t spiInitErr = spi_bus_initialize(host_id, &buscfg, SPI_DMA_CH_AUTO);
  if (spiInitErr == ESP_ERR_INVALID_STATE) {
    jll_error("%u Esp32EthernetNetwork - SPI bus already initialized", timeMillis());
    spiInitErr = ESP_OK;
  }
  ESP_ERROR_CHECK(spiInitErr);

  // The SPI Ethernet module(s) do not have a MAC address, so we instead add one to the Wi-Fi MAC.
  uint8_t wifi_mac_addr[6];
  ESP_ERROR_CHECK(esp_read_mac(wifi_mac_addr, ESP_MAC_EFUSE_FACTORY));
  NetworkDeviceId wifi_mac(wifi_mac_addr);
  localDeviceId_ = wifi_mac.PlusOne();

  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = host_id;
  phy_config.reset_gpio_num = kEthernetPinReset;

  spi_device_interface_config_t spi_devcfg = {
      .mode = 0,
      .clock_speed_hz = 20 * 1000 * 1000,
      .spics_io_num = kEthernetPinCS,
      .queue_size = 20,
  };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(host_id, &spi_devcfg);
  if (kEthernetPinInterrupt < 0) { w5500_config.poll_period_ms = 10; }
#else
  spi_device_handle_t spi_handle = nullptr;
  ESP_ERROR_CHECK(spi_bus_add_device(host_id, &spi_devcfg, &spi_handle));
  eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(spi_handle);
#endif
  w5500_config.int_gpio_num = kEthernetPinInterrupt;
  esp_eth_mac_t* mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
  esp_eth_phy_t* phy = esp_eth_phy_new_w5500(&phy_config);

  esp_eth_handle_t eth_handle = nullptr;
  esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac, phy);
  ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config_spi, &eth_handle));

  ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, localDeviceId_.data()));

  InitializeNetStack();

  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
  esp_netif_t* eth_netif = esp_netif_new(&cfg);
  ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));

  esp_event_handler_instance_t ethInstance;
  esp_event_handler_instance_t ipInstance;
  // Register event handlers on the default event loop. That runs in task "sys_evt" on core 0 (and is not configurable).
  ESP_ERROR_CHECK(esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &ethInstance));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &EventHandler, this, &ipInstance));

  ESP_ERROR_CHECK(esp_eth_start(eth_handle));
  ESP_ERROR_CHECK(esp_netif_init());

  if (inet_pton(AF_INET, DefaultMulticastAddress(), &multicastAddress_) != 1) {
    jll_fatal("Esp32EthernetNetwork failed to parse multicast address");
  }

  // This task needs to be pinned to core 0 since that's where the system event handler runs (see above).
  if (xTaskCreatePinnedToCore(TaskFunction, "JL_Eth", configMINIMAL_STACK_SIZE + 2000,
                              /*parameters=*/this, kHighestTaskPriority, &taskHandle_, /*coreID=*/0) != pdPASS) {
    jll_fatal("Failed to create Esp32EthernetNetwork task");
  }

  jll_info("%u Esp32EthernetNetwork initialized ethernet with MAC address " DEVICE_ID_FMT, timeMillis(),
           DEVICE_ID_HEX(localDeviceId_));
}

// static
Esp32EthernetNetwork* Esp32EthernetNetwork::get() {
  static Esp32EthernetNetwork sSingleton;
  return &sSingleton;
}

Esp32EthernetNetwork::~Esp32EthernetNetwork() {
  jll_fatal("Destructing Esp32EthernetNetwork is not currently supported");
  // This destruction is unsafe since a race condition could cause the event handler to fire after the destructor is
  // called. If we ever have a need to destroy this, we'll need to make this safe first.
  // CloseSocket();
}

}  // namespace jazzlights

#endif  // JL_ESP32_ETHERNET
