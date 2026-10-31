#include "jazzlights/network/esp32_nimble.h"

#ifdef ESP32
#if !JL_DISABLE_BLUETOOTH && JL_ESP32_NIMBLE

#include <esp_bt.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <string>

#include "jazzlights/util/instrumentation.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/pseudorandom.h"

#if JL_ARDUINO
// This is an Arduino header and in theory we shouldn't need it, but see comment near btStarted() below.
#include <esp32-hal-bt.h>
#endif  // JL_ARDUINO

#if !JL_ESP32S3 && !JL_ESP32C3 && !JL_ESP32C6
#define JL_BLE4 1
#else
#define JL_BLE4 0
#endif

namespace jazzlights {
namespace {

// NimBLE stores addresses in little-endian order, whereas Bluedroid and our NetworkDeviceId use big-endian.
NetworkDeviceId DeviceIdFromNimBleAddress(const uint8_t* nimbleAddress) {
  uint8_t address[NetworkDeviceId::size()];
  static_assert(NetworkDeviceId::size() == sizeof(ble_addr_t::val), "bad size");
  for (size_t i = 0; i < NetworkDeviceId::size(); i++) { address[i] = nimbleAddress[NetworkDeviceId::size() - 1 - i]; }
  return NetworkDeviceId(address);
}

}  // namespace

// static
std::atomic<bool> Esp32NimBleNetwork::sHostResetPending_(false);

void Esp32NimBleNetwork::StartScanning() {
  ESP32_BLE_DEBUG("Starting scan...");
  if (state_ != State::kIdle) { jll_error("Unexpected state %s starting scan", StateToString(state_).c_str()); }
  struct ble_gap_disc_params discParams = {};
  discParams.itvl = 16000;    // 10s (unit is 625us).
  discParams.window = 16000;  // 10s (unit is 625us).
  discParams.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
  discParams.limited = 0;
  discParams.passive = 1;
  discParams.filter_duplicates = 0;
  // Scan forever, we stop scanning when we need to send.
  JL_LOG_IF_SLOW("ble_gap_disc");
  const int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &discParams, &GapEventCallback, this);
  if (rc != 0) {
    jll_error("Failed to start scanning: %d", rc);
    state_ = State::kIdle;
    return;
  }
  ESP32_BLE_DEBUG("Scanning has now started");
  state_ = State::kScanning;
  StopScanningIn(UnpredictableRandom::GetNumberBetween(500000, 1000000));  // 0.5-1s.
}

void Esp32NimBleNetwork::StopScanning() {
  ESP32_BLE_DEBUG("Stopping scan...");
  if (state_ != State::kScanning) { jll_error("Unexpected state %s stopping scan", StateToString(state_).c_str()); }
  JL_LOG_IF_SLOW("ble_gap_disc_cancel");
  const int rc = ble_gap_disc_cancel();
  // BLE_HS_EALREADY means that scanning had already stopped, for example due to a host reset.
  if (rc != 0 && rc != BLE_HS_EALREADY) { jll_error("Failed to stop scanning: %d", rc); }
  ESP32_BLE_DEBUG("Scanning has now stopped");
  state_ = State::kIdle;
}

void Esp32NimBleNetwork::StartAdvertising() {
  ESP32_BLE_DEBUG("StartAdvertising");
  if (state_ != State::kIdle) { jll_error("Unexpected state %s starting advertising", StateToString(state_).c_str()); }
  uint8_t advPayload[kMaxAdvPayloadLength];
  const size_t advPayloadLength = GetNextAdvPayloadToSend(advPayload);
  int rc;
  {
    JL_LOG_IF_SLOW("ble_gap_adv_set_data");
    rc = ble_gap_adv_set_data(advPayload, advPayloadLength);
  }
  if (rc != 0) {
    jll_error("Failed to set advertising data: %d", rc);
    state_ = State::kIdle;
    return;
  }
  struct ble_gap_adv_params advParams = {};
  // We advertise as connectable because the Bluedroid implementation does, and it ignores non-connectable
  // advertisements. Since our advertising data is raw, disc_mode does not get added to it.
  advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
  advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
  advParams.itvl_min = 0x20;
  advParams.itvl_max = 0x40;
  advParams.channel_map = 0x07;  // All three advertising channels.
  advParams.filter_policy = BLE_HCI_ADV_FILT_NONE;
  {
    JL_LOG_IF_SLOW("ble_gap_adv_start");
    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, /*direct_addr=*/nullptr, BLE_HS_FOREVER, &advParams, &GapEventCallback,
                           this);
  }
  if (rc != 0) {
    jll_error("Failed to start advertising: %d", rc);
    state_ = State::kIdle;
    return;
  }
  ESP32_BLE_DEBUG("Advertising has now started");
  state_ = State::kAdvertising;
  StopAdvertisingIn(5 * kMicrosecondsPerMillisecond);
}

void Esp32NimBleNetwork::StopAdvertising() {
  ESP32_BLE_DEBUG("StopAdvertising");
  if (state_ != State::kAdvertising) {
    jll_error("Unexpected state %s stopping advertising", StateToString(state_).c_str());
  }
  JL_LOG_IF_SLOW("ble_gap_adv_stop");
  const int rc = ble_gap_adv_stop();
  // BLE_HS_EALREADY means that advertising had already stopped, for example because someone connected to us.
  if (rc != 0 && rc != BLE_HS_EALREADY) { jll_error("Failed to stop advertising: %d", rc); }
  ESP32_BLE_DEBUG("Advertising has now stopped");
  state_ = State::kIdle;
}

void Esp32NimBleNetwork::MaybeUpdateAdvertisingState() {
  std::unique_lock<std::mutex> radioLock(radioMutex_, std::defer_lock);
  {
    JL_LOG_IF_SLOW("NimBLE radioMutex_ wait");
    radioLock.lock();
  }
  if (!ble_hs_synced()) { return; }
  if (sHostResetPending_.exchange(false, std::memory_order_relaxed)) {
    // The host reset cancelled any ongoing scanning or advertising.
    state_ = State::kIdle;
  }
  if (state_ == State::kIdle) {
    // This happens on startup, after a host reset, or if starting to scan or advertise failed.
    StartScanning();
    return;
  }
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
  if (shouldStopAdvertising) {
    StopAdvertising();
    if (ExtractShouldTriggerSendAsap()) {
      StartAdvertising();
    } else {
      StartScanning();
    }
  }
  if (shouldStopScanning) {
    StopScanning();
    StartAdvertising();
  }
}

// static
int Esp32NimBleNetwork::GapEventCallback(struct ble_gap_event* event, void* arg) {
  const Microseconds callbackTime = TimeMicros();
  static_cast<Esp32NimBleNetwork*>(arg)->HandleGapEvent(event, callbackTime);
  return 0;
}

void Esp32NimBleNetwork::HandleGapEvent(struct ble_gap_event* event, Microseconds callbackTime) {
  switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
      // Received a scan result.
      const struct ble_gap_disc_desc& desc = event->disc;
      if (desc.length_data < 2 || desc.data[0] <= 1 || desc.data[1] != kAdvType) {
        // This advertisement isn't one of ours, silently ignore it.
        break;
      }
      if (desc.addr.type != BLE_ADDR_PUBLIC || desc.event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND ||
          desc.length_data > BLE_HS_ADV_MAX_SZ) {
        // This advertisement doesn't match what we normally get, this is weird.
        jll_error("Unexpected scan result %s addr_type=%u event_type=%u rssi=%d length_data=%u",
                  DeviceIdFromNimBleAddress(desc.addr.val).ToString().c_str(), desc.addr.type, desc.event_type,
                  desc.rssi, desc.length_data);
        break;
      }
      const NetworkDeviceId sender = DeviceIdFromNimBleAddress(desc.addr.val);
      if (ESP32_BLE_DEBUG_ENABLED()) {
        char advRawData[kMaxAdvDataHexStringSize] = {};
        ConvertToHex(advRawData, sizeof(advRawData), desc.data, desc.length_data);
        ESP32_BLE_DEBUG("Received adv<%u:%s> from %s", desc.length_data, advRawData, sender.ToString().c_str());
      }
      ReceiveAdvertisement(sender, desc.length_data - 2, &desc.data[2], desc.rssi, callbackTime);
    } break;
    case BLE_GAP_EVENT_DISC_COMPLETE: {
      ESP32_BLE_DEBUG("Scanning has completed with reason %d", event->disc_complete.reason);
    } break;
    case BLE_GAP_EVENT_ADV_COMPLETE: {
      ESP32_BLE_DEBUG("Advertising has completed with reason %d", event->adv_complete.reason);
    } break;
    case BLE_GAP_EVENT_CONNECT: {
      // Someone connected to us since we advertise as connectable. We don't support connections so disconnect
      // right away. The connection stopped advertising, MaybeUpdateAdvertisingState() will move on to scanning.
      if (event->connect.status != 0) { break; }
      jll_info("Terminating unexpected BLE connection");
      const int rc = ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
      if (rc != 0) { jll_error("Failed to terminate BLE connection: %d", rc); }
    } break;
    default: {
      ESP32_BLE_DEBUG("GAP callback fired with unknown event=%d!", event->type);
    } break;
  }
}

// static
void Esp32NimBleNetwork::HostTask(void* /*parameters*/) {
  // This function only returns when nimble_port_stop() is called, which we never do.
  nimble_port_run();
  nimble_port_freertos_deinit();
}

// static
void Esp32NimBleNetwork::HostSyncCallback() { ESP32_BLE_DEBUG("NimBLE host synced"); }

// static
void Esp32NimBleNetwork::HostResetCallback(int reason) {
  jll_error("NimBLE host reset, reason=%d", reason);
  sHostResetPending_.store(true, std::memory_order_relaxed);
}

// static
NetworkDeviceId Esp32NimBleNetwork::InitBluetoothStackAndQueryLocalDeviceId() {
#if JL_NIMBLE_ARDUINO
  // NimBLE-Arduino doesn't initialize the controller in nimble_port_init(), so we do it here the same way
  // Esp32BleNetwork does. ESP-IDF's nimble_port_init() handles this itself.
  ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if JL_BLE4
  cfg.mode = ESP_BT_MODE_BLE;
#endif  // JL_BLE4
  ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
#if CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE
  ESP_ERROR_CHECK(esp_nimble_hci_init());
#endif  // CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE
#endif  // JL_NIMBLE_ARDUINO
  ESP_ERROR_CHECK(nimble_port_init());

#if JL_BLE4 && JL_ARDUINO
  // If we remove the next line, or if we replace the condition with `if (false)`, then the call to
  // esp_bt_controller_init() above fails and returns ESP_ERR_INVALID_STATE. This call to btStarted() ensures that
  // arduino uses the right btInUse(). In theory, all btStarted() does is checking esp_bt_controller_get_status(),
  // but checking that doesn't work. When we switch from arduino to espidf, this should go away because arduino is
  // what's currently releasing Bluetooth memory from under us when it thinks btInUse() is false.
  // https://github.com/espressif/arduino-esp32/issues/3436#issuecomment-927341016
  if (esp_random() == 0xdeadbeef) { (void)btStarted(); }
#endif  // JL_BLE4 && JL_ARDUINO

  ble_hs_cfg.sync_cb = &HostSyncCallback;
  ble_hs_cfg.reset_cb = &HostResetCallback;
  nimble_port_freertos_init(&HostTask);

  // Wait for the host and controller to sync before we can query the address. Scanning will then start from the first
  // call to RunLoop().
  while (!ble_hs_synced()) { vTaskDelay(1); }

  // Initialize localDeviceId_.
  uint8_t localAddress[NetworkDeviceId::size()] = {};
  const int rc = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, localAddress, /*out_is_nrpa=*/nullptr);
  if (rc != 0) { jll_fatal("Failed to query local BLE address: %d", rc); }
  const NetworkDeviceId localDeviceId = DeviceIdFromNimBleAddress(localAddress);
  jll_info("Initialized NimBLE with local MAC address %s", localDeviceId.ToString().c_str());
  return localDeviceId;
}

std::string Esp32NimBleNetwork::StateToString(Esp32NimBleNetwork::State state) {
#define CASE_STATE_RETURN_STRING(_case) \
  case State::k##_case: return #_case
  switch (state) {
    CASE_STATE_RETURN_STRING(Idle);
    CASE_STATE_RETURN_STRING(Scanning);
    CASE_STATE_RETURN_STRING(Advertising);
  }
  return "Unknown";
#undef CASE_STATE_RETURN_STRING
}

// static
Esp32NimBleNetwork* Esp32NimBleNetwork::Get() {
  static Esp32NimBleNetwork sStaticInstance;
  return &sStaticInstance;
}

}  // namespace jazzlights

#endif  // !JL_DISABLE_BLUETOOTH && JL_ESP32_NIMBLE
#endif  // ESP32
