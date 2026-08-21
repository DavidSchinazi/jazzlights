#include "esp32_ble.h"

#ifdef ESP32
#if !JL_DISABLE_BLUETOOTH

#include <cmath>
#include <string>
#include <unordered_map>

#include "jazzlights/network/ble_payload.h"
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

// static
NetworkDeviceId Esp32BleNetwork::InitBluetoothStackAndQueryLocalDeviceId() {
  NimBLEDevice::init("");
  NetworkDeviceId localDeviceId(NimBLEDevice::getAddress().getVal());
  jll_info("Initialized BLE with local MAC address %s", localDeviceId.toString().c_str());
  return localDeviceId;
}

Esp32BleNetwork::Esp32BleNetwork() {
  scan_ = NimBLEDevice::getScan();
  scan_->setScanCallbacks(&scanCallbacks_);
  scan_->setActiveScan(false);       // Passive scan, matches esp_ble_scan_params_t.scan_type from before.
  scan_->setDuplicateFilter(false);  // Matches BLE_SCAN_DUPLICATE_DISABLE from before.
  // NimBLEScan::setInterval()/setWindow() take milliseconds directly (unlike Bluedroid's
  // scan_interval/scan_window, which were in 625us units). The previous values were
  // 16000 * 625us = 10s each.
  scan_->setInterval(10000);
  scan_->setWindow(10000);

  advertising_ = NimBLEDevice::getAdvertising();
  advertising_->setMinInterval(0x20);
  advertising_->setMaxInterval(0x40);
  advertising_->setScanFilter(false, false);

  StartScanning();
}

void Esp32BleNetwork::ScanCallbacks::onResult(const NimBLEAdvertisedDevice* dev) {
  const std::vector<uint8_t>& payload = dev->getPayload();
  const uint8_t* innerPayload = nullptr;
  uint8_t innerLen = 0;
  if (!FindJazzLightsAdStructure(payload.data(), payload.size(), &innerPayload, &innerLen)) {
    // This advertisement isn't one of ours, silently ignore it.
    return;
  }
  if (ESP32_BLE_DEBUG_ENABLED()) {
    char advRawData[31 * 2 + 1] = {};
    convertToHex(advRawData, sizeof(advRawData), payload.data(), static_cast<uint8_t>(payload.size()));
    ESP32_BLE_DEBUG("Received adv<%zu:%s> from %s", payload.size(), advRawData, dev->getAddress().toString().c_str());
  }
  owner_->ReceiveAdvertisement(NetworkDeviceId(dev->getAddress().getVal()), innerLen, innerPayload, dev->getRSSI());
}

void Esp32BleNetwork::StartScanning() {
  ESP32_BLE_DEBUG("Starting scan...");
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kScanning;
    timeToStopScanning_ = timeMillis() + UnpredictableRandom::GetNumberBetween(500, 1000);
  }
  scan_->start(0, false);  // duration=0 means "scan forever," we stop it manually.
}

void Esp32BleNetwork::StartAdvertising() {
  uint8_t advPayload[kBleMaxInnerPayloadLength + 2];
  uint8_t innerPayloadSize = GetNextInnerPayloadToSend(&advPayload[2], kBleMaxInnerPayloadLength);
  if (innerPayloadSize > kBleMaxInnerPayloadLength) {
    jll_error("GetNextInnerPayloadToSend returned nonsense %u", innerPayloadSize);
    innerPayloadSize = kBleMaxInnerPayloadLength;
    memset(advPayload, 0, sizeof(advPayload));
  }
  advPayload[0] = 1 + innerPayloadSize;
  advPayload[1] = kBleAdvType;
  if (ESP32_BLE_DEBUG_ENABLED()) {
    char advRawData[(kBleMaxInnerPayloadLength + 2) * 2 + 1] = {};
    convertToHex(advRawData, sizeof(advRawData), advPayload, 2 + innerPayloadSize);
    ESP32_BLE_DEBUG("Sending adv<%u:%s>", 2 + innerPayloadSize, advRawData);
  }
  NimBLEAdvertisementData advData;
  // Raw AD-structure append -- NOT setManufacturerData()/setServiceData(), which would prepend
  // their own AD type bytes (0xFF/0x16) and break wire compatibility with the kBleAdvType framing.
  advData.addData(advPayload, 2 + innerPayloadSize);
  advertising_->setAdvertisementData(advData);
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    state_ = State::kAdvertising;
    timeToStopAdvertising_ = timeMillis() + 5;
  }
  ESP32_BLE_DEBUG("StartAdvertising");
  advertising_->start(0);  // duration=0 means indefinite, we stop it manually.
}

void Esp32BleNetwork::MaybeUpdateAdvertisingState() {
  const Milliseconds currentTime = timeMillis();
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
  // mutex_ is released before touching scan_/advertising_ -- never call their start()/stop() while
  // holding the lock.
  if (shouldStopAdvertising) {
    ESP32_BLE_DEBUG("StopAdvertising");
    advertising_->stop();
    if (ExtractShouldTriggerSendAsap()) {
      StartAdvertising();
    } else {
      StartScanning();
    }
  }
  if (shouldStopScanning) {
    ESP32_BLE_DEBUG("Stopping scan...");
    scan_->stop();
    StartAdvertising();
  }
}

std::list<NetworkMessage> Esp32BleNetwork::getReceivedMessagesImpl() {
  std::list<NetworkMessage> results;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    results = std::move(receivedMessages_);
    receivedMessages_.clear();
  }
  return results;
}

void Esp32BleNetwork::triggerSendAsap() {
  ESP32_BLE_DEBUG("TriggerSendAsap");
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    numUrgentSends_ = 10;
  }
  MaybeUpdateAdvertisingState();
}

void Esp32BleNetwork::setMessageToSend(const NetworkMessage& messageToSend) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!hasDataToSend_ || !messageToSend_.isEqualExceptOriginationTime(messageToSend)) {
    ESP32_BLE_DEBUG("Setting messageToSend %s", networkMessageToString(messageToSend).c_str());
    ESP32_BLE_DEBUG("Old messageToSend was %s", networkMessageToString(messageToSend_).c_str());
  }
  hasDataToSend_ = true;
  messageToSend_ = messageToSend;
}

void Esp32BleNetwork::disableSending() {
  const std::lock_guard<std::mutex> lock(mutex_);
  hasDataToSend_ = false;
}

void Esp32BleNetwork::ReceiveAdvertisement(const NetworkDeviceId& deviceIdentifier, uint8_t innerPayloadLength,
                                           const uint8_t* innerPayload, int rssi) {
  const Milliseconds currentTime = timeMillis();
  NetworkMessage message;
  Milliseconds receiptTime;
  if (!DecodeBleInnerPayload(deviceIdentifier, innerPayload, innerPayloadLength, rssi, currentTime, &message,
                             &receiptTime)) {
    return;
  }

  ESP32_BLE_DEBUG("Received %s", networkMessageToString(message).c_str());
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

uint8_t Esp32BleNetwork::GetNextInnerPayloadToSend(uint8_t* innerPayload, uint8_t maxInnerPayloadLength) {
  NetworkMessage messageToSend;
  Milliseconds currentTime = timeMillis();
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    messageToSend = messageToSend_;
  }
  uint8_t innerPayloadLength = EncodeBleInnerPayload(messageToSend, currentTime, innerPayload, maxInnerPayloadLength);
  if (ESP32_BLE_DEBUG_ENABLED() && innerPayloadLength > 0) {
    char advRawData[kBleMaxInnerPayloadLength * 2 + 1] = {};
    convertToHex(advRawData, sizeof(advRawData), innerPayload, innerPayloadLength);
    ESP32_BLE_DEBUG("Setting inner payload to <%u:%s>", innerPayloadLength, advRawData);
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

void Esp32BleNetwork::runLoopImpl() { MaybeUpdateAdvertisingState(); }

std::string Esp32BleNetwork::getStatusStr() {
  const Milliseconds currentTime = timeMillis();
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
