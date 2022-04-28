#include "esp32_ble.hpp"

#if ESP32_BLE

#include <unordered_map>
#include <cmath>
#include <string>

#include "unisparks/util/log.hpp"

#ifndef ESP32_BLE_DEBUG_OVERRIDE
#  define ESP32_BLE_DEBUG_OVERRIDE 0
#endif // ESP32_BLE_DEBUG_OVERRIDE

#if ESP32_BLE_DEBUG_OVERRIDE
#  define ESP32_BLE_DEBUG(...) info(__VA_ARGS__)
#  define ESP32_BLE_DEBUG_ENABLED() 1
#else // ESP32_BLE_DEBUG_OVERRIDE
#  define ESP32_BLE_DEBUG(...) debug(__VA_ARGS__)
#  define ESP32_BLE_DEBUG_ENABLED() is_debug_logging_enabled()
#endif // ESP32_BLE_DEBUG_OVERRIDE

namespace unisparks {
namespace {

constexpr uint8_t kAdvType = 0xDF;

void writeUint32(uint8_t* data, uint32_t number) {
  data[0] = static_cast<uint8_t>((number & 0xFF000000) >> 24);
  data[1] = static_cast<uint8_t>((number & 0x00FF0000) >> 16);
  data[2] = static_cast<uint8_t>((number & 0x0000FF00) >> 8);
  data[3] = static_cast<uint8_t>((number & 0x000000FF));
}

void writeUint16(uint8_t* data, uint16_t number) {
  data[0] = static_cast<uint8_t>((number & 0xFF00) >> 8);
  data[1] = static_cast<uint8_t>((number & 0x00FF));
}

uint32_t readUint32(const uint8_t* data) {
  return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
}

uint16_t readUint16(const uint8_t* data) {
  return (data[0] << 8) | (data[1]);
}

void convertToHex(char* target, size_t targetLength,
                  const uint8_t* source, uint8_t sourceLength) {
  if (targetLength <= sourceLength * 2) {
    return;
  }
	for (int i = 0; i < sourceLength; i++) {
		sprintf(target, "%.2x", (char)*source);
		source++;
		target += 2;
	}
  *target = '\0';
}

} // namespace

void Esp32BleNetwork::UpdateState(Esp32BleNetwork::State expectedCurrentState,
                                  Esp32BleNetwork::State newState) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (state_ != expectedCurrentState) {
    error("Unexpected state %s updating from %s to %s",
          StateToString(state_).c_str(),
          StateToString(expectedCurrentState).c_str(),
          StateToString(newState).c_str());
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
  advParams.adv_int_min       = 0x20;
  advParams.adv_int_max       = 0x40;
  advParams.adv_type          = ADV_TYPE_IND;
  advParams.own_addr_type     = BLE_ADDR_TYPE_PUBLIC;
  advParams.peer_addr_type    = BLE_ADDR_TYPE_PUBLIC;
	advParams.channel_map       = ADV_CHNL_ALL;
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
    if (state_ == State::kAdvertising &&
        timeToStopAdvertising_ > 0 &&
        currentTime >= timeToStopAdvertising_) {
      timeToStopAdvertising_ = 0;
      shouldStopAdvertising = true;
    } else if (state_ == State::kScanning && isSendingEnabled_ && hasDataToSend_ &&
               (numUrgentSends_ > 0 ||
                 (timeToStopScanning_ > 0 &&
                  currentTime >= timeToStopScanning_))) {
      if (numUrgentSends_ > 0) {
        numUrgentSends_--;
      }
      timeToStopScanning_ = 0;
      shouldStopScanning = true;
    }
  }
  if (shouldStopAdvertising) {
    StopAdvertising(currentTime);
  }
  if (shouldStopScanning) {
    StopScanning(currentTime);
  }
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

void Esp32BleNetwork::setMessageToSend(const NetworkMessage& messageToSend,
                        Milliseconds currentTime) {
  uint8_t blePayload[3 + 6 + 2 + 4 + 4 + 4] = {0xFF, 'L', '1'};
  static_assert(sizeof(blePayload) <= kMaxInnerPayloadLength, "bad size");
  messageToSend.originator.writeTo(&blePayload[3]);
  writeUint16(&blePayload[3 + 6], messageToSend.precedence);
  writeUint32(&blePayload[3 + 6 + 2], messageToSend.currentPattern);
  writeUint32(&blePayload[3 + 6 + 2 + 4], messageToSend.nextPattern);
  constexpr uint8_t kTimeByteOffset = 3 + 6 + 2 + 4 + 4;
  Milliseconds timeSubtract = currentTime - messageToSend.elapsedTime;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    hasDataToSend_ = true;
    innerPayloadLength_ = sizeof(blePayload);
    memcpy(innerPayload_, blePayload, sizeof(blePayload));
    timeByteOffset_ = kTimeByteOffset;
    timeSubtract_ = timeSubtract;
    if (ESP32_BLE_DEBUG_ENABLED()) {
      char advRawData[sizeof(blePayload) * 2 + 1] = {};
      convertToHex(advRawData, sizeof(advRawData),
                  innerPayload_, sizeof(blePayload));
      ESP32_BLE_DEBUG("%u Setting inner payload to <%u:%s>",
            currentTime, sizeof(blePayload), advRawData);
    }
  }
}

void Esp32BleNetwork::ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier,
                                    uint8_t innerPayloadLength,
                                    const uint8_t* innerPayload,
                                    int /*rssi*/,
                                    Milliseconds currentTime) {
  if (innerPayloadLength > kMaxInnerPayloadLength) {
    error("%u Received advertisement with unexpected length %u",
          currentTime, innerPayloadLength);
    return;
  }
  if (innerPayloadLength < 3 + 6 + 2 + 4 + 4 + 4) {
    info("%u Ignoring received BLE with unexpected length %u",
         currentTime, innerPayloadLength);
    return;
  }
  static constexpr uint8_t prefix[3] = {0xFF, 'L', '1'};
  if (memcmp(innerPayload, prefix, sizeof(prefix)) != 0) {
    info("%u Ignoring received BLE with unexpected prefix", currentTime);
    return;
  }
  NetworkMessage message;
  message.sender = deviceIdentifier;
  message.originator = NetworkDeviceId(&innerPayload[3]);
  message.precedence = readUint16(&innerPayload[3 + 6]);
  message.currentPattern = readUint32(&innerPayload[3 + 6 + 2]);
  message.receiptTime = currentTime;
  message.elapsedTime = readUint32(&innerPayload[3 + 6 + 2 + 4 + 4]);

  // Empirical measurements with the ATOM Matrix show a RTT of 50ms,
  // so we offset the one way transmission time by half that.
  constexpr Milliseconds transmissionOffset = 25;
  if (message.receiptTime > transmissionOffset) {
    message.receiptTime -= transmissionOffset;
  } else {
    message.receiptTime = 0;
  }

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

uint8_t Esp32BleNetwork::GetNextInnerPayloadToSend(uint8_t* innerPayload,
                                            uint8_t maxInnerPayloadLength,
                                            Milliseconds currentTime) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (innerPayloadLength_ == 0) {
    return 0;
  }
  if (innerPayloadLength_ > maxInnerPayloadLength) {
    error("%u GetNextInnerPayloadToSend nonsense %u > %u",
          currentTime, innerPayloadLength_, maxInnerPayloadLength);
    return 0;
  }
  memcpy(innerPayload, innerPayload_, innerPayloadLength_);
  if (timeByteOffset_ + sizeof(uint32_t) <= innerPayloadLength_) {
    Milliseconds currentTime = millis();
    Milliseconds timeToSend;
    if (timeSubtract_ <= currentTime) {
      timeToSend = currentTime - timeSubtract_;
    } else {
      timeToSend = 0xFFFFFFFF;
    }
    writeUint32(&innerPayload[timeByteOffset_], timeToSend);
  }
  return innerPayloadLength_;
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
  uint8_t innerPayloadSize = GetNextInnerPayloadToSend(&advPayload[2],
                                                       kMaxInnerPayloadLength,
                                                       currentTime);
  if (innerPayloadSize > kMaxInnerPayloadLength) {
    error("%u getNextAdvertisementToSend returned nonsense %u",
          currentTime, innerPayloadSize);
    innerPayloadSize = kMaxInnerPayloadLength;
    memset(advPayload, 0, sizeof(advPayload));
  }
  advPayload[0] = 1 + innerPayloadSize;
  advPayload[1] = kAdvType;
  if (ESP32_BLE_DEBUG_ENABLED()) {
    char advRawData[(2 + innerPayloadSize) * 2 + 1] = {};
    convertToHex(advRawData, sizeof(advRawData),
                advPayload, 2 + innerPayloadSize);
    ESP32_BLE_DEBUG("%u Sending adv<%u:%s>", currentTime, 2 + innerPayloadSize, advRawData);
  }
  ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(advPayload, 2 + innerPayloadSize));
}

void Esp32BleNetwork::GapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  get()->GapCallbackInner(event, param, timeMillis());
}

void Esp32BleNetwork::GapCallbackInner(esp_gap_ble_cb_event_t event,
                                esp_ble_gap_cb_param_t *param,
                                Milliseconds currentTime) {
  switch (event) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      switch(param->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT: {
          // Received a scan result.
          if (param->scan_rst.adv_data_len < 2 ||
              param->scan_rst.ble_adv[0] <= 1 ||
              param->scan_rst.ble_adv[1] != kAdvType) {
            // This advertisement isn't one of ours, silently ignore it.
            break;
          }
          if (param->scan_rst.ble_addr_type != BLE_ADDR_TYPE_PUBLIC ||
              param->scan_rst.ble_evt_type != ESP_BLE_EVT_CONN_ADV ||
              param->scan_rst.flag != 0 || param->scan_rst.num_resps != 1 ||
              param->scan_rst.adv_data_len > ESP_BLE_ADV_DATA_LEN_MAX ||
              param->scan_rst.scan_rsp_len > ESP_BLE_SCAN_RSP_DATA_LEN_MAX) {
            // This advertisement doesn't match what we normally get, this is weird.
            char macAddressString[18] = {};
            snprintf(macAddressString, sizeof(macAddressString), "%02x:%02x:%02x:%02x:%02x:%02x",
                    param->scan_rst.bda[0], param->scan_rst.bda[1], param->scan_rst.bda[2],
                    param->scan_rst.bda[3], param->scan_rst.bda[4], param->scan_rst.bda[5]);
            error("%u Unexpected scan result %s dev_type=%d ble_addr_type=%d"
                  " ble_evt_type=%d rssi=%d flag=%d num_resps=%d adv_data_len=%u"
                  " scan_rsp_len=%u num_dis=%u",
                  currentTime, macAddressString, param->scan_rst.dev_type,
                  param->scan_rst.ble_addr_type, param->scan_rst.ble_evt_type,
                  param->scan_rst.rssi, param->scan_rst.flag,
                  param->scan_rst.num_resps, param->scan_rst.adv_data_len,
                  param->scan_rst.scan_rsp_len, param->scan_rst.num_dis);
            break;
          }
          if (ESP32_BLE_DEBUG_ENABLED()) {
            char advRawData[31 * 2 + 1] = {};
            convertToHex(advRawData, sizeof(advRawData),
                        param->scan_rst.ble_adv, param->scan_rst.adv_data_len);
            ESP32_BLE_DEBUG("%u Received adv<%u:%s> from " ESP_BD_ADDR_STR,
                  currentTime, param->scan_rst.adv_data_len, advRawData,
                  ESP_BD_ADDR_HEX(param->scan_rst.bda));
          }
          ReceiveAdvertisement(NetworkDeviceId(param->scan_rst.bda),
                               param->scan_rst.adv_data_len - 2,
                               &param->scan_rst.ble_adv[2],
                               param->scan_rst.rssi,
                               currentTime);
        } break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT: {
          ESP32_BLE_DEBUG("%u Scanning has now stopped via ESP_GAP_SEARCH_INQ_CMPL_EVT",
                currentTime);
          UpdateState(State::kStoppingScan, State::kIdle);
          StartConfigureAdvertising(currentTime);
        } break;
        default: {
          ESP32_BLE_DEBUG("%u GAP scan event %d!",
                currentTime, param->scan_rst.search_evt);
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
      StopScanningIn(random(500, 1000));
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
  info("Initialized BLE with local MAC address " ESP_BD_ADDR_STR " (type %u)",
       ESP_BD_ADDR_HEX(localAddress), addressType);
  localDeviceId_ = NetworkDeviceId(localAddress);
  // Override callbacks away from BLEDevice back to us.
  ESP_ERROR_CHECK(esp_ble_gap_register_callback(&Esp32BleNetwork::GapCallback));
  // Configure scanning parameters.
  esp_ble_scan_params_t scanParams = {};
  scanParams.scan_type          = BLE_SCAN_TYPE_PASSIVE;
  scanParams.own_addr_type      = BLE_ADDR_TYPE_PUBLIC;
  scanParams.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  scanParams.scan_interval      = 16000; // 10s (unit is 625us).
  scanParams.scan_window        = 16000; // 10s (unit is 625us).
	scanParams.scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE;
  ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scanParams));
}

Esp32BleNetwork* Esp32BleNetwork::get() {
  static Esp32BleNetwork static_instance;
  return &static_instance;
}

void Esp32BleNetwork::runLoopImpl(Milliseconds currentTime) {
  MaybeUpdateAdvertisingState(currentTime);
}

std::string Esp32BleNetwork::StateToString(Esp32BleNetwork::State state) {
#define CASE_STATE_RETURN_STRING(_case) \
  case State::k ## _case: return #_case
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

}  // namespace unisparks

#endif // ESP32_BLE
