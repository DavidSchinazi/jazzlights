// The server that extras/scripts/test_secure_channel.py tests, see CMakeLists.txt.
//
// Usage: secure-channel-test-server [--no-partition] [--pending-verify] [--image-out <path>]
//                                   [--rollback-guard <mode>] [--rollback-impossible] [--free-heap <bytes>]
//                                   <hex public key>...
//
// --rollback-guard runs OtaRollbackGuard with shorter times, where <mode> says what the fake primary runloop reports:
// working, no-network, no-render, late-network, or hung to not run it at all.
//
// --free-heap sets how much free heap the channel sees, which is 1GB otherwise.

#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "jazzlights/network/ota_handler.h"
#include "jazzlights/network/secure_channel_echo.h"
#include "jazzlights/network/secure_channel_server.h"

namespace {

constexpr int kRestartExitCode = 42;
constexpr int kRollbackExitCode = 43;
constexpr uint32_t kPartitionSize = 1024 * 1024;
// Where these are in an actual firmware image.
constexpr size_t kImageChipIdOffset = 12;
constexpr size_t kImageProjectNameOffset = 80;
constexpr uint8_t kImageMagic = 0xe9;
const char kRunningProject[] = "jazzlights";

const esp_partition_t gRunningPartition = {kPartitionSize};
const esp_partition_t gUpdatePartition = {kPartitionSize};
bool gHasPartition = true;
std::atomic<bool> gPendingVerify{false};
bool gRollbackImpossible = false;
std::string gImageOutPath;
size_t gFreeHeap = 1024 * 1024 * 1024;
std::vector<uint8_t> gPartitionData;
bool gWriting = false;

}  // namespace

const char* esp_err_to_name(esp_err_t err) { return err == ESP_OK ? "ESP_OK" : "ESP_FAIL"; }

const esp_app_desc_t* esp_app_get_description() {
  static esp_app_desc_t sDescription;
  strncpy(sDescription.project_name, kRunningProject, sizeof(sDescription.project_name));
  return &sDescription;
}

void esp_restart() {
  printf("Restarting\n");
  fflush(stdout);
  _exit(kRestartExitCode);
}

const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t* /*start*/) {
  return gHasPartition ? &gUpdatePartition : nullptr;
}

const esp_partition_t* esp_ota_get_running_partition() { return &gRunningPartition; }

esp_err_t esp_ota_get_state_partition(const esp_partition_t* /*partition*/, esp_ota_img_states_t* state) {
  *state = gPendingVerify ? ESP_OTA_IMG_PENDING_VERIFY : ESP_OTA_IMG_VALID;
  return ESP_OK;
}

esp_err_t esp_ota_begin(const esp_partition_t* /*partition*/, size_t imageSize, esp_ota_handle_t* handle) {
  if (gPendingVerify || gWriting || imageSize != OTA_WITH_SEQUENTIAL_WRITES) { return ESP_FAIL; }
  gWriting = true;
  gPartitionData.clear();
  *handle = 1;
  return ESP_OK;
}

esp_err_t esp_ota_write(esp_ota_handle_t /*handle*/, const void* data, size_t size) {
  if (!gWriting) { return ESP_FAIL; }
  const uint8_t* bytes = static_cast<const uint8_t*>(data);
  gPartitionData.insert(gPartitionData.end(), bytes, bytes + size);
  return ESP_OK;
}

esp_err_t esp_ota_end(esp_ota_handle_t /*handle*/) {
  if (!gWriting) { return ESP_FAIL; }
  gWriting = false;
  // The image header says 0 for the classic ESP32, which is what we fake.
  if (gPartitionData.size() <= kImageChipIdOffset + 1 || gPartitionData[0] != kImageMagic ||
      gPartitionData[kImageChipIdOffset] != 0 || gPartitionData[kImageChipIdOffset + 1] != 0) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t esp_ota_abort(esp_ota_handle_t /*handle*/) {
  if (!gWriting) { return ESP_FAIL; }
  gWriting = false;
  gPartitionData.clear();
  return ESP_OK;
}

esp_err_t esp_ota_get_partition_description(const esp_partition_t* /*partition*/, esp_app_desc_t* description) {
  memset(description, 0, sizeof(*description));
  if (gPartitionData.size() < kImageProjectNameOffset + sizeof(description->project_name)) { return ESP_FAIL; }
  memcpy(description->project_name, gPartitionData.data() + kImageProjectNameOffset, sizeof(description->project_name));
  return ESP_OK;
}

esp_err_t esp_ota_set_boot_partition(const esp_partition_t* /*partition*/) {
  if (!gImageOutPath.empty()) {
    FILE* file = fopen(gImageOutPath.c_str(), "wb");
    if (file == nullptr) { return ESP_FAIL; }
    fwrite(gPartitionData.data(), 1, gPartitionData.size(), file);
    fclose(file);
  }
  return ESP_OK;
}

esp_err_t esp_ota_mark_app_valid_cancel_rollback() {
  gPendingVerify = false;
  return ESP_OK;
}

esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot() {
  if (gRollbackImpossible) { return ESP_FAIL; }
  printf("Rolling back\n");
  fflush(stdout);
  _exit(kRollbackExitCode);
}

size_t heap_caps_get_free_size(uint32_t /*caps*/) { return gFreeHeap; }
size_t heap_caps_get_largest_free_block(uint32_t /*caps*/) { return gFreeHeap; }

int main(int argc, char** argv) {
  using namespace jazzlights;
  // Writing to a socket that the test closed must not kill us.
  signal(SIGPIPE, SIG_IGN);
  setvbuf(stdout, nullptr, _IOLBF, 0);
  std::vector<SecureChannelPublicKey> keys;
  std::string rollbackGuardMode;
  for (int i = 1; i < argc; i++) {
    const std::string arg = argv[i];
    if (arg == "--no-partition") {
      gHasPartition = false;
    } else if (arg == "--pending-verify") {
      gPendingVerify = true;
    } else if (arg == "--image-out") {
      gImageOutPath = argv[++i];
    } else if (arg == "--rollback-guard") {
      rollbackGuardMode = argv[++i];
    } else if (arg == "--rollback-impossible") {
      gRollbackImpossible = true;
    } else if (arg == "--free-heap") {
      gFreeHeap = std::stoul(argv[++i]);
    } else {
      if (arg.size() != 2 * kSecureChannelPublicKeySize) {
        fprintf(stderr, "Invalid public key %s\n", arg.c_str());
        return 1;
      }
      SecureChannelPublicKey key;
      for (size_t j = 0; j < key.size(); j++) {
        key[j] = static_cast<uint8_t>(std::stoi(arg.substr(2 * j, 2), nullptr, 16));
      }
      keys.push_back(key);
    }
  }
  SecureChannelServer* server = SecureChannelServer::Get();
  static OtaHandler otaHandler(server);
  const uint8_t localDeviceId[NetworkDeviceId::size()] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
  otaHandler.SetLocalDeviceId(NetworkDeviceId(localDeviceId));
  static SecureChannelEchoHandler echoHandler;
  server->RegisterHandler(OtaHandler::kFirstType, OtaHandler::kLastType, &otaHandler);
  server->RegisterHandler(SecureChannelEchoHandler::kEchoType, SecureChannelEchoHandler::kEchoType, &echoHandler);
  server->Start(keys.data(), keys.size());
  if (rollbackGuardMode.empty()) {
    while (true) { pause(); }
  }
  constexpr Microseconds kConnectedTime = 1 * kMicrosecondsPerSecond;
  constexpr Microseconds kTimeout = 3 * kMicrosecondsPerSecond;
  constexpr Microseconds kLateNetworkTime = 4 * kMicrosecondsPerSecond;
  static OtaRollbackGuard guard(kConnectedTime, kTimeout);
  guard.Start();
  if (rollbackGuardMode == "hung") {
    while (true) { pause(); }
  }
  const Microseconds start = TimeMicros();
  while (true) {
    const bool rendered = rollbackGuardMode != "no-render";
    const bool connected = rollbackGuardMode == "working" || rollbackGuardMode == "no-render" ||
                           (rollbackGuardMode == "late-network" && TimeMicros() - start >= kLateNetworkTime);
    guard.RunLoop(rendered, connected);
    usleep(10000);
  }
}
