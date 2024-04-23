#include "esp32_ble.h"

#ifdef ARDUINO
#if !JL_DISABLE_BLUETOOTH

#include <cmath>
#include <string>
#include <unordered_map>

#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"

#ifndef ESP32_BLE_DEBUG_OVERRIDE
#define ESP32_BLE_DEBUG_OVERRIDE 0
#endif  // ESP32_BLE_DEBUG_OVERRIDE

#if ESP32_BLE_DEBUG_OVERRIDE
#define ESP32_BLE_DEBUG(...) jll_info(__VA_ARGS__)
#define ESP32_BLE_DEBUG_ENABLED() 1
#else  // ESP32_BLE_DEBUG_OVERRIDE
#define ESP32_BLE_DEBUG(...) jll_debug(__VA_ARGS__)
#define ESP32_BLE_DEBUG_ENABLED() is_debug_logging_enabled()
#endif  // ESP32_BLE_DEBUG_OVERRIDE

namespace jazzlights {
namespace {

// Squat on an unused Bluetooth Advertising Data Type.
// https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/core/ad_types.yaml
// https://www.bluetooth.com/specifications/assigned-numbers/
constexpr uint8_t kAdvType = 0x96;

void convertToHex(char* target, size_t targetLength, const uint8_t* source, uint8_t sourceLength) {
  if (targetLength <= static_cast<size_t>(sourceLength) * 2) { return; }
  for (uint8_t i = 0; i < sourceLength; i++) {
    sprintf(target, "%.2x", (char)*source);
    source++;
    target += 2;
  }
  *target = '\0';
}

}  // namespace

void Esp32BleNetwork::UpdateState(Esp32BleNetwork::State expectedCurrentState, Esp32BleNetwork::State newState) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (state_ != expectedCurrentState) {
    jll_error("Unexpected state %s updating from %s to %s", StateToString(state_).c_str(),
              StateToString(expectedCurrentState).c_str(), StateToString(newState).c_str());
  }
  state_ = newState;
}

void Esp32BleNetwork::StartScanning(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u Starting scan...", currentTime);
  UpdateState(State::kIdle, State::kStartingScan);
  // Set scan duration to one hour so it never stops unless we request it to.
  // For some reasons very high values do not work.
  constexpr uint32_t kScanDurationSeconds = 3600;
  ESP_ERROR_CHECK(esp_ble_gap_start_scanning(kScanDurationSeconds));
}

void Esp32BleNetwork::StopScanning(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u Stopping scan...", currentTime);
  UpdateState(State::kScanning, State::kStoppingScan);
  ESP_ERROR_CHECK(esp_ble_gap_stop_scanning());
}

void Esp32BleNetwork::StartAdvertising(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u StartAdvertising", currentTime);
  UpdateState(State::kConfiguringAdvertising, State::kStartingAdvertising);
  esp_ble_adv_params_t advParams = {};
  advParams.adv_int_min = 0x20;
  advParams.adv_int_max = 0x40;
  advParams.adv_type = ADV_TYPE_IND;
  advParams.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  advParams.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;
  advParams.channel_map = ADV_CHNL_ALL;
  advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  ESP_ERROR_CHECK(esp_ble_gap_start_advertising(&advParams));
}

void Esp32BleNetwork::StopAdvertising(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u StopAdvertising", currentTime);
  UpdateState(State::kAdvertising, State::kStoppingAdvertising);
  ESP_ERROR_CHECK(esp_ble_gap_stop_advertising());
}

constexpr size_t Esp32BleNetwork::kMaxInnerPayloadLength;

void Esp32BleNetwork::MaybeUpdateAdvertisingState(Milliseconds currentTime) {
  bool shouldStopAdvertising = false;
  bool shouldStopScanning = false;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kAdvertising && timeToStopAdvertising_ > 0 && currentTime >= timeToStopAdvertising_) {
      timeToStopAdvertising_ = 0;
      shouldStopAdvertising = true;
    } else if (state_ == State::kScanning && isSendingEnabled_ && hasDataToSend_ &&
               (numUrgentSends_ > 0 || (timeToStopScanning_ > 0 && currentTime >= timeToStopScanning_))) {
      if (numUrgentSends_ > 0) { numUrgentSends_--; }
      timeToStopScanning_ = 0;
      shouldStopScanning = true;
    }
  }
  if (shouldStopAdvertising) { StopAdvertising(currentTime); }
  if (shouldStopScanning) { StopScanning(currentTime); }
}

void Esp32BleNetwork::StopAdvertisingIn(Milliseconds duration) {
  const std::lock_guard<std::mutex> lock(mutex_);
  timeToStopAdvertising_ = millis() + duration;
}

void Esp32BleNetwork::StopScanningIn(Milliseconds duration) {
  const std::lock_guard<std::mutex> lock(mutex_);
  timeToStopScanning_ = millis() + duration;
}

std::list<NetworkMessage> Esp32BleNetwork::getReceivedMessagesImpl(Milliseconds currentTime) {
  std::list<NetworkMessage> results;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    results = std::move(receivedMessages_);
    receivedMessages_.clear();
  }
  return results;
}

void Esp32BleNetwork::triggerSendAsap(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u TriggerSendAsap", currentTime);
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    numUrgentSends_ = 10;
  }
  MaybeUpdateAdvertisingState(currentTime);
}

void Esp32BleNetwork::setMessageToSend(const NetworkMessage& messageToSend, Milliseconds currentTime) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!hasDataToSend_ || !messageToSend_.isEqualExceptOriginationTime(messageToSend)) {
    ESP32_BLE_DEBUG("%u Setting messageToSend %s", currentTime,
                    networkMessageToString(messageToSend, currentTime).c_str());
    ESP32_BLE_DEBUG("%u Old messageToSend was %s", currentTime,
                    networkMessageToString(messageToSend_, currentTime).c_str());
  }
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void Esp32BleNetwork::disableSending(Milliseconds currentTime) {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

constexpr uint8_t kOriginatorOffset = 0;
constexpr uint8_t kPrecedenceOffset = kOriginatorOffset + 6;
constexpr uint8_t kNumHopsOffset = kPrecedenceOffset + 2;
constexpr uint8_t kOriginationTimeOffset = kNumHopsOffset + 1;
constexpr uint8_t kCurrentPatternOffset = kOriginationTimeOffset + 2;
constexpr uint8_t kNextPatternOffset = kCurrentPatternOffset + 4;
constexpr uint8_t kPatternTimeOffset = kNextPatternOffset + 4;
constexpr uint8_t kPayloadLength = kPatternTimeOffset + 2;

void Esp32BleNetwork::ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier, uint8_t innerPayloadLength,
                                           const uint8_t* innerPayload, int /*rssi*/, Milliseconds currentTime) {
  if (innerPayloadLength > kMaxInnerPayloadLength) {
    jll_error("%u Received advertisement with unexpected length %u", currentTime, innerPayloadLength);
    return;
  }
  if (innerPayloadLength < kPayloadLength) {
    ESP32_BLE_DEBUG("%u Ignoring received BLE with unexpected length %u", currentTime, innerPayloadLength);
    return;
  }
  NetworkMessage message;
  message.sender = deviceIdentifier;
  message.originator = NetworkDeviceId(&innerPayload[kOriginatorOffset]);
  message.precedence = readUint16(&innerPayload[kPrecedenceOffset]);
  message.numHops = innerPayload[kNumHopsOffset];
  Milliseconds originationTimeDelta = readUint16(&innerPayload[kOriginationTimeOffset]);
  message.currentPattern = readUint32(&innerPayload[kCurrentPatternOffset]);
  message.nextPattern = readUint32(&innerPayload[kNextPatternOffset]);
  Milliseconds patternTimeDelta = readUint16(&innerPayload[kPatternTimeOffset]);

  // Empirical measurements with the ATOM Matrix show a RTT of 50ms,
  // so we offset the one way transmission time by half that.
  constexpr Milliseconds kTransmissionOffset = 25;
  Milliseconds receiptTime;
  if (currentTime > kTransmissionOffset) {
    receiptTime = currentTime - kTransmissionOffset;
  } else {
    receiptTime = 0;
  }
  if (receiptTime >= patternTimeDelta) {
    message.currentPatternStartTime = receiptTime - patternTimeDelta;
  } else {
    message.currentPatternStartTime = 0;
  }
  if (receiptTime >= originationTimeDelta) {
    message.lastOriginationTime = receiptTime - originationTimeDelta;
  } else {
    message.lastOriginationTime = 0;
  }

  ESP32_BLE_DEBUG("%u Received %s", currentTime, networkMessageToString(message, currentTime).c_str());
  lastReceiveTime_ = receiptTime;

  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (receivedMessages_.size() > 100) {
      // Make sure we do not run out of memory if no one is
      // periodically calling GetScanResults().
      receivedMessages_.clear();
    }
    receivedMessages_.push_back(message);
  }
}

uint8_t Esp32BleNetwork::GetNextInnerPayloadToSend(uint8_t* innerPayload, uint8_t maxInnerPayloadLength,
                                                   Milliseconds currentTime) {
  const std::lock_guard<std::mutex> lock(mutex_);
  static_assert(kPayloadLength <= kMaxInnerPayloadLength, "bad size");
  if (kPayloadLength > maxInnerPayloadLength) {
    jll_error("%u GetNextInnerPayloadToSend nonsense %u > %u", currentTime, kPayloadLength, maxInnerPayloadLength);
    return 0;
  }

  uint16_t originationTimeDelta;
  if (messageToSend_.lastOriginationTime <= currentTime && currentTime - messageToSend_.lastOriginationTime <= 0xFFFF) {
    originationTimeDelta = currentTime - messageToSend_.lastOriginationTime;
  } else {
    originationTimeDelta = 0xFFFF;
  }
  uint16_t patternTimeDelta;
  if (messageToSend_.currentPatternStartTime <= currentTime &&
      currentTime - messageToSend_.currentPatternStartTime <= 0xFFFF) {
    patternTimeDelta = currentTime - messageToSend_.currentPatternStartTime;
  } else {
    patternTimeDelta = 0xFFFF;
  }

  messageToSend_.originator.writeTo(&innerPayload[kOriginatorOffset]);
  writeUint16(&innerPayload[kPrecedenceOffset], messageToSend_.precedence);
  innerPayload[kNumHopsOffset] = messageToSend_.numHops;
  writeUint16(&innerPayload[kOriginationTimeOffset], originationTimeDelta);
  writeUint32(&innerPayload[kCurrentPatternOffset], messageToSend_.currentPattern);
  writeUint32(&innerPayload[kNextPatternOffset], messageToSend_.nextPattern);
  writeUint16(&innerPayload[kPatternTimeOffset], patternTimeDelta);

  if (ESP32_BLE_DEBUG_ENABLED()) {
    char advRawData[kPayloadLength * 2 + 1] = {};
    convertToHex(advRawData, sizeof(advRawData), innerPayload, kPayloadLength);
    ESP32_BLE_DEBUG("%u Setting inner payload to <%u:%s>", currentTime, kPayloadLength, advRawData);
  }
  return kPayloadLength;
}

bool Esp32BleNetwork::ExtractShouldTriggerSendAsap() {
  bool shouldTriggerSendAsap;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (numUrgentSends_ > 0) {
      numUrgentSends_--;
      shouldTriggerSendAsap = true;
    } else {
      shouldTriggerSendAsap = false;
    }
  }
  return shouldTriggerSendAsap;
}

void Esp32BleNetwork::StartConfigureAdvertising(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u StartConfigureAdvertising", currentTime);
  UpdateState(State::kIdle, State::kConfiguringAdvertising);
  uint8_t advPayload[kMaxInnerPayloadLength + 2];
  uint8_t innerPayloadSize = GetNextInnerPayloadToSend(&advPayload[2], kMaxInnerPayloadLength, currentTime);
  if (innerPayloadSize > kMaxInnerPayloadLength) {
    jll_error("%u getNextAdvertisementToSend returned nonsense %u", currentTime, innerPayloadSize);
    innerPayloadSize = kMaxInnerPayloadLength;
    memset(advPayload, 0, sizeof(advPayload));
  }
  advPayload[0] = 1 + innerPayloadSize;
  advPayload[1] = kAdvType;
  if (ESP32_BLE_DEBUG_ENABLED()) {
    char advRawData[(2 + innerPayloadSize) * 2 + 1] = {};
    convertToHex(advRawData, sizeof(advRawData), advPayload, 2 + innerPayloadSize);
    ESP32_BLE_DEBUG("%u Sending adv<%u:%s>", currentTime, 2 + innerPayloadSize, advRawData);
  }
  ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(advPayload, 2 + innerPayloadSize));
}

void Esp32BleNetwork::GapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
  get()->GapCallbackInner(event, param, timeMillis());
}

void Esp32BleNetwork::GapCallbackInner(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param,
                                       Milliseconds currentTime) {
  switch (event) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      switch (param->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT: {
          // Received a scan result.
          if (param->scan_rst.adv_data_len < 2 || param->scan_rst.ble_adv[0] <= 1 ||
              param->scan_rst.ble_adv[1] != kAdvType) {
            // This advertisement isn't one of ours, silently ignore it.
            break;
          }
          if (param->scan_rst.ble_addr_type != BLE_ADDR_TYPE_PUBLIC ||
              param->scan_rst.ble_evt_type != ESP_BLE_EVT_CONN_ADV || param->scan_rst.flag != 0 ||
              param->scan_rst.num_resps != 1 || param->scan_rst.adv_data_len > ESP_BLE_ADV_DATA_LEN_MAX ||
              param->scan_rst.scan_rsp_len > ESP_BLE_SCAN_RSP_DATA_LEN_MAX) {
            // This advertisement doesn't match what we normally get, this is weird.
            char macAddressString[18] = {};
            snprintf(macAddressString, sizeof(macAddressString), "%02x:%02x:%02x:%02x:%02x:%02x",
                     param->scan_rst.bda[0], param->scan_rst.bda[1], param->scan_rst.bda[2], param->scan_rst.bda[3],
                     param->scan_rst.bda[4], param->scan_rst.bda[5]);
            jll_error(
                "%u Unexpected scan result %s dev_type=%d ble_addr_type=%d"
                " ble_evt_type=%d rssi=%d flag=%d num_resps=%d adv_data_len=%u"
                " scan_rsp_len=%u num_dis=%u",
                currentTime, macAddressString, param->scan_rst.dev_type, param->scan_rst.ble_addr_type,
                param->scan_rst.ble_evt_type, param->scan_rst.rssi, param->scan_rst.flag, param->scan_rst.num_resps,
                param->scan_rst.adv_data_len, param->scan_rst.scan_rsp_len, param->scan_rst.num_dis);
            break;
          }
          if (ESP32_BLE_DEBUG_ENABLED()) {
            char advRawData[31 * 2 + 1] = {};
            convertToHex(advRawData, sizeof(advRawData), param->scan_rst.ble_adv, param->scan_rst.adv_data_len);
            ESP32_BLE_DEBUG("%u Received adv<%u:%s> from " ESP_BD_ADDR_STR, currentTime, param->scan_rst.adv_data_len,
                            advRawData, ESP_BD_ADDR_HEX(param->scan_rst.bda));
          }
          ReceiveAdvertisement(NetworkDeviceId(param->scan_rst.bda), param->scan_rst.adv_data_len - 2,
                               &param->scan_rst.ble_adv[2], param->scan_rst.rssi, currentTime);
        } break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT: {
          ESP32_BLE_DEBUG("%u Scanning has now stopped via ESP_GAP_SEARCH_INQ_CMPL_EVT", currentTime);
          UpdateState(State::kStoppingScan, State::kIdle);
          StartConfigureAdvertising(currentTime);
        } break;
        default: {
          ESP32_BLE_DEBUG("%u GAP scan event %d!", currentTime, param->scan_rst.search_evt);
        } break;
      }
    } break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("%u Scan params set", currentTime);
      StartScanning(currentTime);
    } break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("%u Scanning has now started", currentTime);
      UpdateState(State::kStartingScan, State::kScanning);
      StopScanningIn(UnpredictableRandom::GetNumberBetween(500, 1000));
    } break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("%u Scanning has now stopped", currentTime);
      UpdateState(State::kStoppingScan, State::kIdle);
      StartConfigureAdvertising(currentTime);
    } break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("%u Advertising params set", currentTime);
      StartAdvertising(currentTime);
    } break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("%u Advertising has now started", currentTime);
      UpdateState(State::kStartingAdvertising, State::kAdvertising);
      StopAdvertisingIn(5);
    } break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("%u Advertising has now stopped", currentTime);
      UpdateState(State::kStoppingAdvertising, State::kIdle);
      if (ExtractShouldTriggerSendAsap()) {
        StartConfigureAdvertising(currentTime);
      } else {
        StartScanning(currentTime);
      }
    } break;
    default: {
      ESP32_BLE_DEBUG("%u GAP callback fired with unknown event=%d!", currentTime, event);
    } break;
  }
}

NetworkStatus Esp32BleNetwork::update(NetworkStatus status, Milliseconds currentTime) {
  if (status == INITIALIZING || status == CONNECTING) {
    const std::lock_guard<std::mutex> lock(mutex_);
    isSendingEnabled_ = true;
    return CONNECTED;
  } else if (status == DISCONNECTING) {
    const std::lock_guard<std::mutex> lock(mutex_);
    isSendingEnabled_ = false;
    return DISCONNECTED;
  } else {
    return status;
  }
}

Esp32BleNetwork::Esp32BleNetwork() {
  // Let Arduino BLEDevice handle initialization.
  BLEDevice::init("");
  // Initialize localDeviceId_.
  uint8_t addressType;
  esp_bd_addr_t localAddress;
  memset(localAddress, 0, sizeof(localAddress));
  ESP_ERROR_CHECK(esp_ble_gap_get_local_used_addr(localAddress, &addressType));
  jll_info("Initialized BLE with local MAC address " ESP_BD_ADDR_STR " (type %u)", ESP_BD_ADDR_HEX(localAddress),
           addressType);
  localDeviceId_ = NetworkDeviceId(localAddress);
  lastReceiveTime_ = -1;
  // Override callbacks away from BLEDevice back to us.
  ESP_ERROR_CHECK(esp_ble_gap_register_callback(&Esp32BleNetwork::GapCallback));
  // Configure scanning parameters.
  esp_ble_scan_params_t scanParams = {};
  scanParams.scan_type = BLE_SCAN_TYPE_PASSIVE;
  scanParams.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  scanParams.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  scanParams.scan_interval = 16000;  // 10s (unit is 625us).
  scanParams.scan_window = 16000;    // 10s (unit is 625us).
  scanParams.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
  ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scanParams));
}

void Esp32BleNetwork::runLoopImpl(Milliseconds currentTime) { MaybeUpdateAdvertisingState(currentTime); }

std::string Esp32BleNetwork::StateToString(Esp32BleNetwork::State state) {
#define CASE_STATE_RETURN_STRING(_case) \
  case State::k##_case: return #_case
  switch (state) {
    CASE_STATE_RETURN_STRING(Invalid);
    CASE_STATE_RETURN_STRING(Idle);
    CASE_STATE_RETURN_STRING(StartingScan);
    CASE_STATE_RETURN_STRING(Scanning);
    CASE_STATE_RETURN_STRING(StoppingScan);
    CASE_STATE_RETURN_STRING(ConfiguringAdvertising);
    CASE_STATE_RETURN_STRING(StartingAdvertising);
    CASE_STATE_RETURN_STRING(Advertising);
    CASE_STATE_RETURN_STRING(StoppingAdvertising);
  }
  return "Unknown";
#undef CASE_STATE_RETURN_STRING
}

std::string Esp32BleNetwork::getStatusStr(Milliseconds currentTime) const {
  char statStr[100] = {};
  const Milliseconds lastRcv = getLastReceiveTime();
  snprintf(statStr, sizeof(statStr) - 1, "%dms", (lastRcv >= 0 ? currentTime - getLastReceiveTime() : -1));
  return std::string(statStr);
}

}  // namespace jazzlights

#endif  // !JL_DISABLE_BLUETOOTH

namespace jazzlights {

// static
Esp32BleNetwork* Esp32BleNetwork::get() {
  static Esp32BleNetwork static_instance;
  return &static_instance;
}

}  // namespace jazzlights

#endif  // ARDUINO
