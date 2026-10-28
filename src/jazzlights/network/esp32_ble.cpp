#include "esp32_ble.h"

#ifdef ESP32
#if !JL_DISABLE_BLUETOOTH && !JL_ESP32_NIMBLE

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_random.h>

#include <cstring>
#include <mutex>
#include <string>

#include "jazzlights/util/log.h"
#include "jazzlights/util/pseudorandom.h"

// This is an Arduino header and in theory we shouldn't need it, but see comment near btStarted() below.
#include <esp32-hal-bt.h>

#if !JL_ESP32S3 && !JL_ESP32C3 && !JL_ESP32C6
#define JL_BLE4 1
#else
#define JL_BLE4 0
#endif

namespace jazzlights {

void Esp32BleNetwork::UpdateState(Esp32BleNetwork::State expectedCurrentState, Esp32BleNetwork::State newState) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (state_ != expectedCurrentState) {
    jll_error("Unexpected state %s updating from %s to %s", StateToString(state_).c_str(),
              StateToString(expectedCurrentState).c_str(), StateToString(newState).c_str());
  }
  state_ = newState;
}

void Esp32BleNetwork::StartScanning() {
  ESP32_BLE_DEBUG("Starting scan...");
  UpdateState(State::kIdle, State::kStartingScan);
  // Set scan duration to one hour so it never stops unless we request it to.
  // For some reasons very high values do not work.
  constexpr uint32_t kScanDurationSeconds = 3600;
  ESP_ERROR_CHECK(esp_ble_gap_start_scanning(kScanDurationSeconds));
}

void Esp32BleNetwork::StopScanning() {
  ESP32_BLE_DEBUG("Stopping scan...");
  UpdateState(State::kScanning, State::kStoppingScan);
  ESP_ERROR_CHECK(esp_ble_gap_stop_scanning());
}

void Esp32BleNetwork::StartAdvertising() {
  ESP32_BLE_DEBUG("StartAdvertising");
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

void Esp32BleNetwork::StopAdvertising() {
  ESP32_BLE_DEBUG("StopAdvertising");
  UpdateState(State::kAdvertising, State::kStoppingAdvertising);
  ESP_ERROR_CHECK(esp_ble_gap_stop_advertising());
}

void Esp32BleNetwork::MaybeUpdateAdvertisingState() {
  bool shouldStopAdvertising = false;
  bool shouldStopScanning = false;
  Microseconds currentTime = TimeMicros();
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == State::kAdvertising && timeToStopAdvertising_ && currentTime >= *timeToStopAdvertising_) {
      timeToStopAdvertising_.reset();
      shouldStopAdvertising = true;
    } else if (state_ == State::kScanning && hasDataToSend_ &&
               (numUrgentSends_ > 0 || (timeToStopScanning_ && currentTime >= *timeToStopScanning_))) {
      if (numUrgentSends_ > 0) { numUrgentSends_--; }
      timeToStopScanning_.reset();
      shouldStopScanning = true;
    }
  }
  if (shouldStopAdvertising) { StopAdvertising(); }
  if (shouldStopScanning) { StopScanning(); }
}

void Esp32BleNetwork::StartConfigureAdvertising() {
  ESP32_BLE_DEBUG("StartConfigureAdvertising");
  UpdateState(State::kIdle, State::kConfiguringAdvertising);
  uint8_t advPayload[kMaxAdvPayloadLength];
  const size_t advPayloadLength = GetNextAdvPayloadToSend(advPayload);
  ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(advPayload, advPayloadLength));
}

void Esp32BleNetwork::GapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
  const Microseconds callbackTime = TimeMicros();
  Get()->GapCallbackInner(event, param, callbackTime);
}

void Esp32BleNetwork::GapCallbackInner(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param,
                                       Microseconds callbackTime) {
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
                "Unexpected scan result %s dev_type=%d ble_addr_type=%d"
                " ble_evt_type=%d rssi=%d flag=%d num_resps=%d adv_data_len=%u"
                " scan_rsp_len=%u num_dis=%lld",
                macAddressString, param->scan_rst.dev_type, param->scan_rst.ble_addr_type, param->scan_rst.ble_evt_type,
                param->scan_rst.rssi, param->scan_rst.flag, param->scan_rst.num_resps, param->scan_rst.adv_data_len,
                param->scan_rst.scan_rsp_len, static_cast<long long>(param->scan_rst.num_dis));
            break;
          }
          if (ESP32_BLE_DEBUG_ENABLED()) {
            char advRawData[kMaxAdvDataHexStringSize] = {};
            ConvertToHex(advRawData, sizeof(advRawData), param->scan_rst.ble_adv, param->scan_rst.adv_data_len);
            ESP32_BLE_DEBUG("Received adv<%u:%s> from " ESP_BD_ADDR_STR, param->scan_rst.adv_data_len, advRawData,
                            ESP_BD_ADDR_HEX(param->scan_rst.bda));
          }
          ReceiveAdvertisement(NetworkDeviceId(param->scan_rst.bda), param->scan_rst.adv_data_len - 2,
                               &param->scan_rst.ble_adv[2], param->scan_rst.rssi, callbackTime);
        } break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT: {
          ESP32_BLE_DEBUG("Scanning has now stopped via ESP_GAP_SEARCH_INQ_CMPL_EVT");
          UpdateState(State::kStoppingScan, State::kIdle);
          StartConfigureAdvertising();
        } break;
        default: {
          ESP32_BLE_DEBUG("GAP scan event %d!", param->scan_rst.search_evt);
        } break;
      }
    } break;
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("Scan params set");
      StartScanning();
    } break;
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("Scanning has now started");
      UpdateState(State::kStartingScan, State::kScanning);
      StopScanningIn(UnpredictableRandom::GetNumberBetween(500000, 1000000));  // 0.5-1s.
    } break;
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("Scanning has now stopped");
      UpdateState(State::kStoppingScan, State::kIdle);
      StartConfigureAdvertising();
    } break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("Advertising params set");
      StartAdvertising();
    } break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("Advertising has now started");
      UpdateState(State::kStartingAdvertising, State::kAdvertising);
      StopAdvertisingIn(5 * kMicrosecondsPerMillisecond);
    } break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      ESP32_BLE_DEBUG("Advertising has now stopped");
      UpdateState(State::kStoppingAdvertising, State::kIdle);
      if (ExtractShouldTriggerSendAsap()) {
        StartConfigureAdvertising();
      } else {
        StartScanning();
      }
    } break;
    default: {
      ESP32_BLE_DEBUG("GAP callback fired with unknown event=%d!", event);
    } break;
  }
}

// static
NetworkDeviceId Esp32BleNetwork::InitBluetoothStackAndQueryLocalDeviceId() {
  // Initialize ESP Bluetooth stack.
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if JL_BLE4
  cfg.mode = ESP_BT_MODE_BLE;
#endif  // JL_BLE4
  ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());

#if JL_BLE4
  // If we remove the next line, or if we replace the condition with `if (false)`, then the call to
  // esp_bt_controller_init() above fails and returns ESP_ERR_INVALID_STATE. This call to btStarted() ensures that
  // arduino uses the right btInUse(). In theory, all btStarted() does is checking esp_bt_controller_get_status(),
  // but checking that doesn't work. When we switch from arduino to espidf, this should go away because arduino is
  // what's currently releasing Bluetooth memory from under us when it thinks btInUse() is false.
  // https://github.com/espressif/arduino-esp32/issues/3436#issuecomment-927341016
  if (esp_random() == 0xdeadbeef) { (void)btStarted(); }
#endif  // JL_BLE4

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

  // Initialize localDeviceId_.
  uint8_t addressType;
  esp_bd_addr_t localAddress;
  memset(localAddress, 0, sizeof(localAddress));
  ESP_ERROR_CHECK(esp_ble_gap_get_local_used_addr(localAddress, &addressType));
  jll_info("Initialized BLE with local MAC address " ESP_BD_ADDR_STR " (type %u)", ESP_BD_ADDR_HEX(localAddress),
           addressType);
  return NetworkDeviceId(localAddress);
}

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

// static
Esp32BleNetwork* Esp32BleNetwork::Get() {
  static Esp32BleNetwork sStaticInstance;
  return &sStaticInstance;
}

}  // namespace jazzlights

#endif  // !JL_DISABLE_BLUETOOTH && !JL_ESP32_NIMBLE

#endif  // ESP32
