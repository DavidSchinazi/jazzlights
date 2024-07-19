#include "jazzlights/networks/arduino_esp32_wifi.h"

#ifdef ESP32
#if JL_ARDUINO_ESP32_WIFI

#include <esp_wifi.h>

#include <sstream>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {
constexpr UBaseType_t kWiFiTaskNotificationIndex = 0;
constexpr TickType_t kWiFiDhcpTimeout = pdMS_TO_TICKS(5000);      // 5 seconds.
constexpr TickType_t kWiFiDhcpRetryTime = pdMS_TO_TICKS(600000);  // 10 minutes.
TaskHandle_t gWiFiTaskHandle = nullptr;
TimerHandle_t gWiFiDhcpTimer = nullptr;
// Notification bits.
enum NotificationBit : uint32_t {
  kWiFiLayerConnected = 1 << 0,
  kWiFiLayerDisconnected = 1 << 1,
  kGotIpAddress = 1 << 2,
  kLostIpAddress = 1 << 3,
};
void NotifyWiFiTask(NotificationBit bit) {
  BaseType_t notifyResult = xTaskGenericNotify(gWiFiTaskHandle, kWiFiTaskNotificationIndex, bit, eSetBits,
                                               /*previous_notification_value=*/nullptr);
  if (notifyResult != pdPASS) { jll_fatal("Wi-Fi xTaskGenericNotify failed"); }
}

void WiFiEventCallback(WiFiEvent_t event, WiFiEventInfo_t /*info*/) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: NotifyWiFiTask(kWiFiLayerConnected); break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: NotifyWiFiTask(kWiFiLayerDisconnected); break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: NotifyWiFiTask(kGotIpAddress); break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP: NotifyWiFiTask(kLostIpAddress); break;
    default: break;
  }
};  // namespace
}  // namespace

// static
void ArduinoEsp32WiFiNetwork::WiFiDhcpTimerCallback(TimerHandle_t /*timer*/) { get()->WiFiDhcpTimerCallbackInner(); }

void ArduinoEsp32WiFiNetwork::WiFiDhcpTimerCallbackInner() {
  const Milliseconds currentTime = timeMillis();
  jll_info("%u %s DHCP timer fired", currentTime, networkName());
  if (attemptingDhcp_) {
    attemptingDhcp_ = false;
    IPAddress ip, gw, snm;
    ip[0] = 169;
    ip[1] = 254;
    ip[2] = UnpredictableRandom::GetNumberBetween(1, 254);
    ip[3] = UnpredictableRandom::GetNumberBetween(0, 255);
    gw.fromString("169.254.0.0");
    snm.fromString("255.255.0.0");
    jll_info("%u %s giving up on DHCP, using %u.%u.%u.%u", currentTime, networkName(), ip[0], ip[1], ip[2], ip[3]);
    WiFi.config(ip, gw, snm);
    BaseType_t timerReset = xTimerChangePeriod(gWiFiDhcpTimer, kWiFiDhcpRetryTime, portMAX_DELAY);
    if (timerReset != pdPASS) { jll_fatal("Wi-Fi DHCP timer retry period set failed"); }
  } else {
    reconnectToWiFi(currentTime, /*force=*/true);
  }
}

NetworkStatus ArduinoEsp32WiFiNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  if (status == INITIALIZING || status == CONNECTING) {
    return CONNECTED;
  } else if (status == DISCONNECTING) {
    return DISCONNECTED;
  } else {
    return status;
  }
}

std::string ArduinoEsp32WiFiNetwork::getStatusStr(Milliseconds currentTime) const {
  // TODO fix race condition
  switch (getStatus()) {
    case INITIALIZING: return "init";
    case DISCONNECTED: return "disconnected";
    case CONNECTING: return "connecting";
    case DISCONNECTING: return "disconnecting";
    case CONNECTION_FAILED: return "failed";
    case CONNECTED: {
      IPAddress ip = WiFi.localIP();
      const Milliseconds lastRcv = getLastReceiveTime();
      char statStr[100] = {};
      snprintf(statStr, sizeof(statStr) - 1, "%s %u.%u.%u.%u - %ums", JL_WIFI_SSID, ip[0], ip[1], ip[2], ip[3],
               (lastRcv >= 0 ? currentTime - getLastReceiveTime() : -1));
      return std::string(statStr);
    }
  }
  return "error";
}

void ArduinoEsp32WiFiNetwork::setMessageToSend(const NetworkMessage& messageToSend, Milliseconds /*currentTime*/) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void ArduinoEsp32WiFiNetwork::disableSending(Milliseconds /*currentTime*/) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

void ArduinoEsp32WiFiNetwork::triggerSendAsap(Milliseconds /*currentTime*/) {}

std::list<NetworkMessage> ArduinoEsp32WiFiNetwork::getReceivedMessagesImpl(Milliseconds /*currentTime*/) {
  std::list<NetworkMessage> results;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    results = std::move(receivedMessages_);
    receivedMessages_.clear();
  }
  return results;
}

void ArduinoEsp32WiFiNetwork::reconnectToWiFi(Milliseconds currentTime, bool force) {
  // Don't do anything if we're already in the process of reconnecting.
  if (reconnecting_ && !force) { return; }
  reconnecting_ = true;
  if (!attemptingDhcp_) {
    attemptingDhcp_ = true;
    jll_info("%u %s going back to another DHCP attempt", currentTime, networkName());
    WiFi.config(IPAddress(), IPAddress(), IPAddress());
  }
  jll_info("%u Initiating Wi-Fi reconnection", currentTime);
  BaseType_t timerStop = xTimerStop(gWiFiDhcpTimer, portMAX_DELAY);
  if (timerStop != pdPASS) { jll_fatal("Wi-Fi DHCP timer stop failed"); }
  WiFi.reconnect();
}

void ArduinoEsp32WiFiNetwork::TaskFunctionInner() {
  const Milliseconds currentTime = timeMillis();
  // Set up event callbacks.
  WiFi.onEvent(WiFiEventCallback, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiEventCallback, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent(WiFiEventCallback, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiEventCallback, ARDUINO_EVENT_WIFI_STA_LOST_IP);
  // Set up DHCP timer.
  gWiFiDhcpTimer = xTimerCreate("wifi_dhcp_timer", kWiFiDhcpTimeout, /*auto_reload=*/pdFALSE,
                                /*timer_id=*/nullptr, WiFiDhcpTimerCallback);
  if (gWiFiDhcpTimer == nullptr) { jll_fatal("Wi-Fi DHCP timer creation failed"); }
  BaseType_t timerStopped = xTimerStop(gWiFiDhcpTimer, portMAX_DELAY);
  if (timerStopped != pdPASS) { jll_fatal("Wi-Fi DHCP timer first stop failed"); }
  // Initialize Wi-Fi stack and asynchronously start connecting.
  WiFi.begin(JL_WIFI_SSID, JL_WIFI_PASSWORD);
  uint8_t macAddress[6] = {};
  localDeviceId_ = NetworkDeviceId(WiFi.macAddress(macAddress));
  jll_info("%u %s " DEVICE_ID_FMT " starting connection to %s", currentTime, networkName(),
           DEVICE_ID_HEX(localDeviceId_), JL_WIFI_SSID);
  while (true) {
    // Wait until we are notified.
    uint32_t notifyValue = 0;
    BaseType_t notifyResult = xTaskGenericNotifyWait(kWiFiTaskNotificationIndex, /*bits_to_clear_on_entry=*/0,
                                                     /*bits_to_clear_on_exit=*/ULONG_MAX, &notifyValue, portMAX_DELAY);
    if (notifyResult != pdPASS) { jll_fatal("Wi-Fi xTaskGenericNotifyWait failed"); }
    const Milliseconds currentTime = timeMillis();
    if ((notifyValue & kWiFiLayerConnected) != 0) {
      reconnecting_ = false;
      jll_info("%u Wi-Fi layer connected", currentTime);
      BaseType_t timerReset = xTimerChangePeriod(gWiFiDhcpTimer, kWiFiDhcpTimeout, portMAX_DELAY);
      if (timerReset != pdPASS) { jll_fatal("Wi-Fi DHCP timer timeout period set failed"); }
    }
    if ((notifyValue & kWiFiLayerDisconnected) != 0) {
      jll_info("%u Wi-Fi layer disconnected", currentTime);
      reconnectToWiFi(currentTime);
    }
    if ((notifyValue & kGotIpAddress) != 0) {
      jll_info("%u Wi-Fi got IP", currentTime);
      BaseType_t timerStop = xTimerStop(gWiFiDhcpTimer, portMAX_DELAY);
      if (timerStop != pdPASS) { jll_fatal("Wi-Fi DHCP timer stop failed"); }
    }
    if ((notifyValue & kLostIpAddress) != 0) {
      jll_info("%u Wi-Fi lost IP", currentTime);
      reconnectToWiFi(currentTime);
    }
  }
}

// static
void ArduinoEsp32WiFiNetwork::TaskFunction(void* /*parameters*/) { get()->TaskFunctionInner(); }

ArduinoEsp32WiFiNetwork::ArduinoEsp32WiFiNetwork() {
  BaseType_t ret = xTaskCreatePinnedToCore(ArduinoEsp32WiFiNetwork::TaskFunction, "Esp32WiFi", /*stack_size=*/4096,
                                           /*parameters=*/nullptr,
                                           /*priority=*/4, &gWiFiTaskHandle, /*coreID=*/1);
  if (ret != pdPASS) { jll_fatal("Failed to create Esp32WiFi task"); }
}

// static
ArduinoEsp32WiFiNetwork* ArduinoEsp32WiFiNetwork::get() {
  static ArduinoEsp32WiFiNetwork sSingleton;
  return &sSingleton;
}

}  // namespace jazzlights

#endif  // JL_ARDUINO_ESP32_WIFI
#endif  // ESP32
