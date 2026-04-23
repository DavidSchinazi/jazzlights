#include "esp32_ble.h"

#ifdef ESP32
#if !JL_DISABLE_BLUETOOTH

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include <string>

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
    sprintf(target, "%.2x", static_cast<unsigned>(*source));
    source++;
    target += 2;
  }
  *target = '\0';
}

// NimBLE stores addresses in little-endian byte order (LSB first).
// We reverse them to MSB-first (network byte order) so MAC addresses are
// human-readable in logs and consistent with payload serialisation.
NetworkDeviceId NimBleAddrToNetworkDeviceId(const uint8_t nimble_addr[6]) {
  uint8_t msb_first[6];
  for (int i = 0; i < 6; i++) { msb_first[i] = nimble_addr[5 - i]; }
  return NetworkDeviceId(msb_first);
}

// State written once from the NimBLE host task during OnSync, then read-only.
static SemaphoreHandle_t s_syncSemaphore = nullptr;
static uint8_t s_ownAddrType = BLE_ADDR_PUBLIC;
static NetworkDeviceId s_localDeviceId;

// Called from the NimBLE host task once it has synced with the controller.
static void OnSync() {
  int rc = ble_hs_id_infer_auto(0, &s_ownAddrType);
  if (rc != 0) {
    jll_error("%u ble_hs_id_infer_auto failed: %d", timeMillis(), rc);
    s_ownAddrType = BLE_ADDR_PUBLIC;
  }
  uint8_t rawAddr[6] = {};
  ble_hs_id_copy_addr(s_ownAddrType, rawAddr, nullptr);
  s_localDeviceId = NimBleAddrToNetworkDeviceId(rawAddr);
  jll_info("%u Initialized NimBLE with local MAC address %s (type %u)", timeMillis(),
           s_localDeviceId.toString().c_str(), s_ownAddrType);
  if (s_syncSemaphore != nullptr) { xSemaphoreGive(s_syncSemaphore); }
}

// Called from the NimBLE host task on BLE stack reset (error recovery).
static void OnReset(int reason) { jll_error("%u NimBLE reset, reason=%d", timeMillis(), reason); }

// The NimBLE host task: runs the port event loop until stopped.
static void NimbleHostTask(void* /*param*/) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

}  // namespace

void Esp32BleNetwork::UpdateState(Esp32BleNetwork::State expectedCurrentState, Esp32BleNetwork::State newState) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (state_ != expectedCurrentState) {
    jll_error("%u Unexpected state %s updating from %s to %s", timeMillis(), StateToString(state_).c_str(),
              StateToString(expectedCurrentState).c_str(), StateToString(newState).c_str());
  }
  state_ = newState;
}

void Esp32BleNetwork::StartScanning(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u Starting scan...", currentTime);
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::kIdle) {
      jll_error("%u StartScanning called in non-idle state %s", currentTime, StateToString(state_).c_str());
      return;
    }
  }
  struct ble_gap_disc_params disc_params = {};
  disc_params.itvl = 16000;           // 10s in 625µs units.
  disc_params.window = 16000;         // 10s in 625µs units.
  disc_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
  disc_params.limited = 0;
  disc_params.passive = 1;
  disc_params.filter_duplicates = 0;
  int rc = ble_gap_disc(s_ownAddrType, BLE_HS_FOREVER, &disc_params, &Esp32BleNetwork::GapEventCallback, nullptr);
  if (rc != 0) {
    jll_error("%u ble_gap_disc failed: %d", currentTime, rc);
    return;
  }
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kScanning;
  }
  StopScanningIn(UnpredictableRandom::GetNumberBetween(500, 1000));
}

void Esp32BleNetwork::StopScanning(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u Stopping scan...", currentTime);
  UpdateState(State::kScanning, State::kStoppingScan);
  int rc = ble_gap_disc_cancel();
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    jll_error("%u ble_gap_disc_cancel failed: %d", currentTime, rc);
  }
}

void Esp32BleNetwork::StopAdvertising(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u StopAdvertising", currentTime);
  UpdateState(State::kAdvertising, State::kStoppingAdvertising);
  int rc = ble_gap_adv_stop();
  if (rc != 0) { jll_error("%u ble_gap_adv_stop failed: %d", currentTime, rc); }
}
}

void Esp32BleNetwork::StartAdvertising(Milliseconds currentTime) {
  ESP32_BLE_DEBUG("%u StartAdvertising", currentTime);
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != State::kIdle) {
      jll_error("%u StartAdvertising called in non-idle state %s", currentTime, StateToString(state_).c_str());
      return;
    }
  }
  // Build raw advertising payload and set it synchronously (no callback).
  uint8_t advPayload[kMaxInnerPayloadLength + 2];
  uint8_t innerPayloadSize = GetNextInnerPayloadToSend(&advPayload[2], kMaxInnerPayloadLength, currentTime);
  if (innerPayloadSize > kMaxInnerPayloadLength) {
    jll_error("%u GetNextInnerPayloadToSend returned nonsense %u", currentTime, innerPayloadSize);
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
  int rc = ble_gap_adv_set_data(advPayload, 2 + innerPayloadSize);
  if (rc != 0) {
    jll_error("%u ble_gap_adv_set_data failed: %d", currentTime, rc);
    return;
  }
  struct ble_gap_adv_params adv_params = {};
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
  adv_params.itvl_min = 0x20;  // 20ms in 625µs units.
  adv_params.itvl_max = 0x40;  // 40ms in 625µs units.
  adv_params.channel_map = 0;  // All channels.
  adv_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
  rc = ble_gap_adv_start(s_ownAddrType, nullptr, BLE_HS_FOREVER, &adv_params, &Esp32BleNetwork::GapEventCallback,
                         nullptr);
  if (rc != 0) {
    jll_error("%u ble_gap_adv_start failed: %d", currentTime, rc);
    return;
  }
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kAdvertising;
  }
  StopAdvertisingIn(5);
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
    } else if (state_ == State::kScanning && hasDataToSend_ &&
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
  timeToStopAdvertising_ = timeMillis() + duration;
}

void Esp32BleNetwork::StopScanningIn(Milliseconds duration) {
  const std::lock_guard<std::mutex> lock(mutex_);
  timeToStopScanning_ = timeMillis() + duration;
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

// originator: 6
// precedence: 2
// numHops: 1
// originationTime: 2
// currentPattern: 4
// nextPattern: 4
// patternTime: 2
// extensionByte: 1 (0x80 = isCreature, 0x40 = isPartying)
// creatureRGB: 3

#if JL_IS_CONFIG(CREATURE)
constexpr uint8_t kMinPayloadLength = 6 + 2 + 1 + 2 + 4 + 4 + 2;
constexpr uint8_t kMaxPayloadLength = kMinPayloadLength + 1 + 3;
constexpr uint8_t kExtensionByteFlagIsCreature = 0x80;
constexpr uint8_t kExtensionByteFlagIsPartying = 0x40;
#else   // CREATURE
constexpr uint8_t kOriginatorOffset = 0;
constexpr uint8_t kPrecedenceOffset = kOriginatorOffset + 6;
constexpr uint8_t kNumHopsOffset = kPrecedenceOffset + 2;
constexpr uint8_t kOriginationTimeOffset = kNumHopsOffset + 1;
constexpr uint8_t kCurrentPatternOffset = kOriginationTimeOffset + 2;
constexpr uint8_t kNextPatternOffset = kCurrentPatternOffset + 4;
constexpr uint8_t kPatternTimeOffset = kNextPatternOffset + 4;
constexpr uint8_t kMinPayloadLength = kPatternTimeOffset + 2;
constexpr uint8_t kMaxPayloadLength = kMinPayloadLength;
#endif  // CREATURE

void Esp32BleNetwork::ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier, uint8_t innerPayloadLength,
                                           const uint8_t* innerPayload, int rssi, Milliseconds currentTime) {
  if (innerPayloadLength > kMaxInnerPayloadLength) {
    jll_error("%u Received advertisement with unexpected length %u", currentTime, innerPayloadLength);
    return;
  }
#if JL_IS_CONFIG(CREATURE)
  if (innerPayloadLength < kMinPayloadLength) {
    jll_error("%u Ignoring received creature BLE with unexpected length %u", currentTime, innerPayloadLength);
    return;
  }
  NetworkReader reader(innerPayload, innerPayloadLength);
  NetworkMessage message;
  message.sender = deviceIdentifier;
  if (!reader.ReadNetworkDeviceId(&message.originator)) {
    jll_error("%u Failed to parse creature originator", currentTime);
    return;
  }
  if (!reader.ReadUint16(&message.precedence)) {
    jll_error("%u Failed to parse creature precedence", currentTime);
    return;
  }
  if (!reader.ReadUint8(&message.numHops)) {
    jll_error("%u Failed to parse creature numHops", currentTime);
    return;
  }
  uint16_t originationTimeDelta16;
  if (!reader.ReadUint16(&originationTimeDelta16)) {
    jll_error("%u Failed to parse creature originationTimeDelta", currentTime);
    return;
  }
  Milliseconds originationTimeDelta = originationTimeDelta16;
  if (!reader.ReadPatternBits(&message.currentPattern)) {
    jll_error("%u Failed to parse creature currentPattern", currentTime);
    return;
  }
  if (!reader.ReadPatternBits(&message.nextPattern)) {
    jll_error("%u Failed to parse creature nextPattern", currentTime);
    return;
  }
  uint16_t patternTimeDelta16;
  if (!reader.ReadUint16(&patternTimeDelta16)) {
    jll_error("%u Failed to parse creature patternTimeDelta", currentTime);
    return;
  }
  Milliseconds patternTimeDelta = patternTimeDelta16;
  uint8_t extensionByte = 0x00;
  if (!reader.Done()) {
    if (!reader.ReadUint8(&extensionByte)) {
      jll_error("%u Failed to parse creature extensionByte", currentTime);
      return;
    }
  }
  message.isCreature = (extensionByte & kExtensionByteFlagIsCreature) != 0;
  if (message.isCreature) {
    message.isPartying = (extensionByte & kExtensionByteFlagIsPartying) != 0;
    uint8_t creatureRed, creatureGreen, creatureBlue;
    if (!reader.ReadUint8(&creatureRed) || !reader.ReadUint8(&creatureGreen) || !reader.ReadUint8(&creatureBlue)) {
      jll_error("%u Failed to parse creature RGB", currentTime);
      return;
    }
    message.creatureColor = (creatureRed << 16) | (creatureGreen << 8) | creatureBlue;
  } else {
    message.creatureColor = 0;
    message.isPartying = false;
  }
  message.receiptRssi = rssi;
  message.receiptTime = currentTime;
#else   // CREATURE
  (void)rssi;
  if (innerPayloadLength < kMinPayloadLength) {
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
#endif  // CREATURE

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
  static_assert(kMaxPayloadLength <= kMaxInnerPayloadLength, "bad size");
  if (kMaxPayloadLength > maxInnerPayloadLength) {
    jll_error("%u GetNextInnerPayloadToSend nonsense %u > %u", currentTime, kMaxPayloadLength, maxInnerPayloadLength);
    return 0;
  }
  uint8_t innerPayloadLength = kMaxPayloadLength;

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

#if JL_IS_CONFIG(CREATURE)
  NetworkWriter writer(innerPayload, maxInnerPayloadLength);
  if (!writer.WriteNetworkDeviceId(messageToSend_.originator)) {
    jll_error("%u Failed to write creature originator", currentTime);
    return 0;
  }
  if (!writer.WriteUint16(messageToSend_.precedence)) {
    jll_error("%u Failed to write creature precedence", currentTime);
    return 0;
  }

  // extensionByte: 1 (0x80 = isCreature, 0x40 = isPartying)
  // creatureRGB: 3

  if (!writer.WriteUint8(messageToSend_.numHops)) {
    jll_error("%u Failed to write creature numHops", currentTime);
    return 0;
  }
  if (!writer.WriteUint16(originationTimeDelta)) {
    jll_error("%u Failed to write creature originationTimeDelta", currentTime);
    return 0;
  }
  if (!writer.WriteUint32(messageToSend_.currentPattern)) {
    jll_error("%u Failed to write creature currentPattern", currentTime);
    return 0;
  }
  if (!writer.WriteUint32(messageToSend_.nextPattern)) {
    jll_error("%u Failed to write creature nextPattern", currentTime);
    return 0;
  }
  if (!writer.WriteUint16(patternTimeDelta)) {
    jll_error("%u Failed to write creature patternTimeDelta", currentTime);
    return 0;
  }
  if (messageToSend_.isCreature) {
    uint8_t extensionByte = kExtensionByteFlagIsCreature;
    if (messageToSend_.isPartying) { extensionByte |= kExtensionByteFlagIsPartying; }
    if (!writer.WriteUint8(extensionByte)) {
      jll_error("%u Failed to write creature extensionByte", currentTime);
      return 0;
    }
    uint8_t creatureRed = (messageToSend_.creatureColor >> 16) & 0xFF;
    uint8_t creatureGreen = (messageToSend_.creatureColor >> 8) & 0xFF;
    uint8_t creatureBlue = messageToSend_.creatureColor & 0xFF;
    if (!writer.WriteUint8(creatureRed) || !writer.WriteUint8(creatureGreen) || !writer.WriteUint8(creatureBlue)) {
      jll_error("%u Failed to write creature RGB", currentTime);
      return 0;
    }
  }
  innerPayloadLength = writer.LengthWritten();

#else   // CREATURE
  messageToSend_.originator.writeTo(&innerPayload[kOriginatorOffset]);
  writeUint16(&innerPayload[kPrecedenceOffset], messageToSend_.precedence);
  innerPayload[kNumHopsOffset] = messageToSend_.numHops;
  writeUint16(&innerPayload[kOriginationTimeOffset], originationTimeDelta);
  writeUint32(&innerPayload[kCurrentPatternOffset], messageToSend_.currentPattern);
  writeUint32(&innerPayload[kNextPatternOffset], messageToSend_.nextPattern);
  writeUint16(&innerPayload[kPatternTimeOffset], patternTimeDelta);
#endif  // CREATURE
  if (ESP32_BLE_DEBUG_ENABLED()) {
    char advRawData[kMaxPayloadLength * 2 + 1] = {};
    convertToHex(advRawData, sizeof(advRawData), innerPayload, kMaxPayloadLength);
    ESP32_BLE_DEBUG("%u Setting inner payload to <%u:%s>", currentTime, kMaxPayloadLength, advRawData);
  }
  return innerPayloadLength;
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
                " scan_rsp_len=%u num_dis=%" PRIu32,
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
  jll_info("%u Initialized BLE with local MAC address " ESP_BD_ADDR_STR " (type %u)", timeMillis(),
           ESP_BD_ADDR_HEX(localAddress), addressType);
  return NetworkDeviceId(localAddress);
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

std::string Esp32BleNetwork::getStatusStr(Milliseconds currentTime) {
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

#endif  // ESP32
