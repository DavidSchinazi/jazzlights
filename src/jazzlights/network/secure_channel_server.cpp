#include "jazzlights/network/secure_channel_server.h"

#if JL_SECURE_CHANNEL

#include <errno.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/inet.h>
#include <lwip/sockets.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <new>

#include "jazzlights/network/tlv.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {
namespace {

enum TopLevelType : uint64_t {
  kTopLevelClientHello = 0,
  kTopLevelServerHello = 1,
  kTopLevelRecord = 2,
};

enum RecordType : uint64_t {
  kRecordClientReady = 0,
  kRecordServerReady = 1,
  kRecordClose = 2,
};

constexpr size_t kCoordinateSize = 32;
constexpr size_t kSignatureSize = 2 * kCoordinateSize;
constexpr size_t kHashSize = 32;
constexpr size_t kGcmKeySize = 16;
constexpr size_t kGcmIvSize = 12;
constexpr size_t kGcmTagSize = 16;
constexpr size_t kKeyMaterialSize = 2 * (kGcmKeySize + kGcmIvSize);
// Type, then a two-byte Length, which holds any size that fits in the send buffer.
constexpr size_t kRecordHeaderMaxSize = 3;
constexpr size_t kSendBufferSize = 4096;
constexpr size_t kMaxPlaintextPerSentRecord = kSendBufferSize - kRecordHeaderMaxSize - kGcmTagSize;
static_assert(kMaxPlaintextPerSentRecord + kGcmTagSize <= kSecureChannelMaxLength);
constexpr size_t kInitialReceiveBufferSize = 256;
// The buffer needs to hold either the ClientHello, or one complete record behind one partial TLV. Their headers can
// take up to kMaxTlvHeaderSize since varints can take any size.
constexpr size_t kMaxReceiveBufferSize =
    std::max(kMaxTlvHeaderSize + kSecureChannelMaxClientHelloLength, 2 * (kMaxTlvHeaderSize + kSecureChannelMaxLength));
constexpr Microseconds kHandshakeTimeout = 10 * kMicrosecondsPerSecond;
constexpr Microseconds kIdleTimeout = 60 * kMicrosecondsPerSecond;
constexpr Microseconds kFlushTimeout = 1 * kMicrosecondsPerSecond;
constexpr int kSendTimeoutMs = 500;
constexpr int kPollTimeoutMs = 1000;
constexpr uint32_t kTaskStackSize = 8192;
// Handshakes take long enough that they must not get in the way of the primary runloop or of the Wi-Fi stack.
constexpr UBaseType_t kTaskPriority = tskIDLE_PRIORITY + 1;
constexpr BaseType_t kTaskCore = 0;

// Each label is followed by a null byte, which sizeof includes.
constexpr char kKeysLabel[] = "JazzLightsSecureChannel v1 keys";
constexpr char kClientReadyLabel[] = "JazzLightsSecureChannel v1 ClientReady";

// Logs how much memory there was, which tells apart running out of it from fragmentation.
void LogAllocationFailure(const char* purpose, size_t size) {
  jll_error("SecureChannel couldn't allocate %zu bytes for %s, with %zu bytes free and a largest free block of %zu",
            size, purpose, heap_caps_get_free_size(MALLOC_CAP_DEFAULT),
            heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

int RandomCallback(void* /*context*/, unsigned char* output, size_t length) {
  esp_fill_random(output, length);
  return 0;
}

// Frees the mbedTLS objects when leaving scope.
struct ScopedP256 {
  ScopedP256() {
    mbedtls_ecp_group_init(&group);
    loaded = mbedtls_ecp_group_load(&group, MBEDTLS_ECP_DP_SECP256R1) == 0;
  }
  ~ScopedP256() { mbedtls_ecp_group_free(&group); }
  mbedtls_ecp_group group;
  bool loaded;
};
struct ScopedPoint {
  ScopedPoint() { mbedtls_ecp_point_init(&point); }
  ~ScopedPoint() { mbedtls_ecp_point_free(&point); }
  mbedtls_ecp_point point;
};
struct ScopedMpi {
  ScopedMpi() { mbedtls_mpi_init(&mpi); }
  ~ScopedMpi() { mbedtls_mpi_free(&mpi); }
  mbedtls_mpi mpi;
};

bool ReadPublicKey(const ScopedP256& p256, const uint8_t* encoded, mbedtls_ecp_point* point) {
  return mbedtls_ecp_point_read_binary(&p256.group, point, encoded, kSecureChannelPublicKeySize) == 0 &&
         mbedtls_ecp_check_pubkey(&p256.group, point) == 0;
}

// HKDF-SHA256 with an empty salt, whose info is the label and both hellos.
bool DeriveKeyMaterial(const uint8_t* sharedSecret, const uint8_t* clientHello, size_t clientHelloLength,
                       const uint8_t* serverHello, size_t serverHelloLength, uint8_t* keyMaterial) {
  const size_t infoLength = sizeof(kKeysLabel) + clientHelloLength + serverHelloLength;
  uint8_t* info = static_cast<uint8_t*>(malloc(infoLength));
  if (info == nullptr) {
    LogAllocationFailure("the HKDF info", infoLength);
    return false;
  }
  memcpy(info, kKeysLabel, sizeof(kKeysLabel));
  memcpy(info + sizeof(kKeysLabel), clientHello, clientHelloLength);
  memcpy(info + sizeof(kKeysLabel) + clientHelloLength, serverHello, serverHelloLength);
  const int result = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), /*salt=*/nullptr, /*salt_len=*/0,
                                  sharedSecret, kCoordinateSize, info, infoLength, keyMaterial, kKeyMaterialSize);
  free(info);
  return result == 0;
}

// One direction of a session's record protection.
struct RecordKeys {
  mbedtls_gcm_context gcm;
  uint8_t iv[kGcmIvSize];
  uint64_t counter = 0;

  RecordKeys() { mbedtls_gcm_init(&gcm); }
  ~RecordKeys() { mbedtls_gcm_free(&gcm); }

  bool Set(const uint8_t* key, const uint8_t* newIv) {
    memcpy(iv, newIv, kGcmIvSize);
    return mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, kGcmKeySize * 8) == 0;
  }

  // Returns false if the counter would wrap.
  bool NextNonce(uint8_t* nonce) {
    if (counter == UINT64_MAX) { return false; }
    memcpy(nonce, iv, kGcmIvSize);
    for (size_t i = 0; i < sizeof(counter); i++) {
      nonce[kGcmIvSize - 1 - i] ^= static_cast<uint8_t>(counter >> (8 * i));
    }
    counter++;
    return true;
  }
};

}  // namespace

class SecureChannelServer::Connection : public SecureSession {
 public:
  enum class State {
    kAwaitingClientHello,
    kAwaitingClientReady,
    kAuthenticated,
    kDraining,  // We've sent all we'll send, and are waiting for that to reach the client.
    kClosed,
  };

  Connection(SecureChannelServer* server, int socket)
      : server_(server), socket_(socket), acceptTime_(TimeMicros()), lastRecordTime_(acceptTime_) {}
  ~Connection() override {
    CloseSocket();
    free(buffer_);
    delete sendKeys_;
    delete receiveKeys_;
    DeleteSignedDataHash();
  }

  int socket() const { return socket_; }
  State state() const { return state_; }
  Microseconds acceptTime() const { return acceptTime_; }
  bool IsHandshaking() const { return state_ == State::kAwaitingClientHello || state_ == State::kAwaitingClientReady; }

  // Reads what's available and handles it. Only called when poll says that the socket is readable.
  void HandleReadable();
  void CheckTimeout(Microseconds now);
  // Closes the connection without telling the client anything. Only for connections that aren't authenticated.
  void Abort() { Fail("was aborted"); }
  // If true, the session has ended and handlers still need to be told, and clears that request.
  bool TakeSessionEnd(bool* clean);
  // Waits for the client to close its side of a connection we're draining. Returns whether it still hasn't.
  bool DiscardWhileDraining();

  // From SecureSession.
  bool Send(uint64_t type, const uint8_t* value, uint64_t length) override;
  void Close(uint64_t status) override;

 private:
  void Fail(const char* reason);
  void CloseSocket();
  void EndSession(bool clean);
  void DeleteSignedDataHash();
  bool Compact(size_t neededCapacity);
  void ProcessRaw();
  void ProcessRecord(const TlvHeader& header);
  void ProcessPlaintext();
  bool HandleClientHello(const uint8_t* tlv, const TlvHeader& header);
  bool HandleClientReady(const uint8_t* value, uint64_t length);
  void HandleClose(const uint8_t* value, uint64_t length);
  bool WriteAll(const uint8_t* data, size_t length);
  bool SendRecord(size_t plaintextLength);
  bool SendTlv(uint64_t type, const uint8_t* value, uint64_t length);

  SecureChannelServer* server_;
  int socket_;
  State state_ = State::kAwaitingClientHello;
  Microseconds acceptTime_;
  Microseconds lastRecordTime_;
  Microseconds drainDeadline_ = 0;
  bool sessionEndPending_ = false;
  bool sessionEndClean_ = false;

  // The receive buffer holds decrypted bytes that we can't handle yet, followed by raw bytes from the socket:
  // | consumed | plaintext | unused | raw | unused |
  //            ^ plaintextStart_    ^ rawStart_    ^ capacity_
  //                        ^ plaintextEnd_  ^ rawEnd_
  uint8_t* buffer_ = nullptr;
  size_t capacity_ = 0;
  size_t plaintextStart_ = 0;
  size_t plaintextEnd_ = 0;
  size_t rawStart_ = 0;
  size_t rawEnd_ = 0;
  // How many upcoming bytes belong to a TLV that's being skipped without being stored.
  size_t rawToSkip_ = 0;
  size_t plaintextToSkip_ = 0;

  RecordKeys* sendKeys_ = nullptr;
  RecordKeys* receiveKeys_ = nullptr;
  mbedtls_sha256_context* signedDataHash_ = nullptr;
};

void SecureChannelServer::Connection::CloseSocket() {
  if (socket_ >= 0) {
    close(socket_);
    socket_ = -1;
  }
}

void SecureChannelServer::Connection::EndSession(bool clean) {
  if (state_ != State::kAuthenticated) { return; }
  sessionEndPending_ = true;
  sessionEndClean_ = clean;
}

bool SecureChannelServer::Connection::TakeSessionEnd(bool* clean) {
  if (!sessionEndPending_) { return false; }
  sessionEndPending_ = false;
  *clean = sessionEndClean_;
  return true;
}

void SecureChannelServer::Connection::DeleteSignedDataHash() {
  if (signedDataHash_ == nullptr) { return; }
  mbedtls_sha256_free(signedDataHash_);
  delete signedDataHash_;
  signedDataHash_ = nullptr;
}

void SecureChannelServer::Connection::Fail(const char* reason) {
  if (state_ == State::kClosed) { return; }
  jll_info("SecureChannel connection %d %s", socket_, reason);
  EndSession(/*clean=*/false);
  CloseSocket();
  state_ = State::kClosed;
}

void SecureChannelServer::Connection::CheckTimeout(Microseconds now) {
  if (IsHandshaking()) {
    if (now - acceptTime_ >= kHandshakeTimeout) { Fail("timed out during its handshake"); }
  } else if (state_ == State::kAuthenticated) {
    if (now - lastRecordTime_ >= kIdleTimeout) { Fail("was idle for too long"); }
  } else if (state_ == State::kDraining) {
    if (now >= drainDeadline_) {
      CloseSocket();
      state_ = State::kClosed;
    }
  }
}

bool SecureChannelServer::Connection::DiscardWhileDraining() {
  uint8_t discarded[128];
  while (state_ == State::kDraining) {
    const ssize_t received = recv(socket_, discarded, sizeof(discarded), /*flags=*/0);
    if (received > 0) { continue; }
    if (received < 0 && errno == EWOULDBLOCK) { return true; }
    CloseSocket();
    state_ = State::kClosed;
  }
  return false;
}

void SecureChannelServer::Connection::HandleReadable() {
  if (state_ == State::kDraining) {
    DiscardWhileDraining();
    return;
  }
  if (state_ == State::kClosed) { return; }
  if (rawEnd_ == capacity_) {
    if (!Compact(std::max(capacity_, kInitialReceiveBufferSize))) { return; }
    // This only grows a little, because ProcessRaw() grows the buffer to fit each record as soon as it knows its size,
    // and memory on the device is tight. At the maximum size this can't be full, because a complete record would have
    // been handled already.
    if (rawEnd_ == capacity_ && !Compact(std::min(capacity_ + kInitialReceiveBufferSize, kMaxReceiveBufferSize))) {
      return;
    }
  }
  const ssize_t received = recv(socket_, buffer_ + rawEnd_, capacity_ - rawEnd_, /*flags=*/0);
  if (received == 0) {
    Fail("was closed by the client without a Close");
    return;
  }
  if (received < 0) {
    if (errno != EWOULDBLOCK) { Fail("failed to read"); }
    return;
  }
  rawEnd_ += static_cast<size_t>(received);
  ProcessRaw();
}

// Moves what we still need to the start of the buffer, and makes sure the buffer has at least this capacity.
bool SecureChannelServer::Connection::Compact(size_t neededCapacity) {
  const size_t plaintextLength = plaintextEnd_ - plaintextStart_;
  const size_t rawLength = rawEnd_ - rawStart_;
  if (plaintextStart_ > 0 && plaintextLength > 0) { memmove(buffer_, buffer_ + plaintextStart_, plaintextLength); }
  if (rawStart_ > plaintextLength && rawLength > 0) {
    memmove(buffer_ + plaintextLength, buffer_ + rawStart_, rawLength);
  }
  plaintextStart_ = 0;
  plaintextEnd_ = plaintextLength;
  rawStart_ = plaintextLength;
  rawEnd_ = plaintextLength + rawLength;
  if (neededCapacity > capacity_) {
    uint8_t* newBuffer = static_cast<uint8_t*>(realloc(buffer_, neededCapacity));
    if (newBuffer == nullptr) {
      LogAllocationFailure("a receive buffer", neededCapacity);
      Fail("couldn't grow its receive buffer");
      return false;
    }
    buffer_ = newBuffer;
    capacity_ = neededCapacity;
  }
  return true;
}

void SecureChannelServer::Connection::ProcessRaw() {
  while (state_ != State::kClosed && state_ != State::kDraining) {
    if (rawToSkip_ > 0) {
      const size_t skipped = std::min(rawToSkip_, rawEnd_ - rawStart_);
      rawStart_ += skipped;
      rawToSkip_ -= skipped;
    }
    if (rawStart_ == rawEnd_) { return; }
    TlvHeader header;
    // Only a ClientHello can be larger, so this needs to look at the type before checking the Length.
    const TlvHeaderStatus status =
        ParseTlvHeader(buffer_ + rawStart_, rawEnd_ - rawStart_, kSecureChannelMaxClientHelloLength, &header);
    if (status == TlvHeaderStatus::kNeedMoreData) { return; }
    if (status == TlvHeaderStatus::kInvalid ||
        (header.type != kTopLevelClientHello && header.length > kSecureChannelMaxLength)) {
      Fail("sent a top-level Length that's too large");
      return;
    }
    // That limits the Length, so it fits in a size_t.
    const size_t tlvLength = header.headerSize + static_cast<size_t>(header.length);
    if (header.type > kTopLevelRecord) {
      // Anyone on the network path can inject these, so they must have no effect on the session.
      rawToSkip_ = tlvLength;
      continue;
    }
    const bool expected = (header.type == kTopLevelClientHello && state_ == State::kAwaitingClientHello) ||
                          (header.type == kTopLevelRecord && state_ != State::kAwaitingClientHello);
    if (!expected) {
      Fail("sent a top-level message out of order");
      return;
    }
    if (rawEnd_ - rawStart_ < tlvLength) {
      // Make sure the rest of it will fit once it arrives.
      if (capacity_ - rawStart_ < tlvLength) { Compact((plaintextEnd_ - plaintextStart_) + tlvLength); }
      return;
    }
    if (header.type == kTopLevelClientHello) {
      if (!HandleClientHello(buffer_ + rawStart_, header)) {
        Fail("sent an invalid ClientHello");
        return;
      }
      rawStart_ += tlvLength;
    } else {
      ProcessRecord(header);
    }
  }
}

void SecureChannelServer::Connection::ProcessRecord(const TlvHeader& header) {
  if (header.length <= kGcmTagSize) {
    Fail("sent a record with no plaintext");
    return;
  }
  // ProcessRaw() limited the Length, so it fits in a size_t.
  const size_t valueLength = static_cast<size_t>(header.length);
  const size_t plaintextLength = valueLength - kGcmTagSize;
  uint8_t aad[kMaxTlvHeaderSize];
  memcpy(aad, buffer_ + rawStart_, header.headerSize);
  // The ciphertext has to move down to directly follow the existing plaintext, and is then decrypted in place. The tag
  // doesn't move, and the record has already been received so the raw bytes that follow it don't either.
  const size_t ciphertextStart = rawStart_ + header.headerSize;
  const size_t recordEnd = ciphertextStart + valueLength;
  uint8_t* plaintext = buffer_ + plaintextEnd_;
  memmove(plaintext, buffer_ + ciphertextStart, plaintextLength);
  uint8_t nonce[kGcmIvSize];
  if (!receiveKeys_->NextNonce(nonce)) {
    Fail("would have wrapped its receive counter");
    return;
  }
  if (mbedtls_gcm_auth_decrypt(&receiveKeys_->gcm, plaintextLength, nonce, sizeof(nonce), aad, header.headerSize,
                               buffer_ + recordEnd - kGcmTagSize, kGcmTagSize, plaintext, plaintext) != 0) {
    Fail("sent a record that failed authentication");
    return;
  }
  // Only authenticated records can extend the session, because anyone can inject top-level TLVs.
  lastRecordTime_ = TimeMicros();
  plaintextEnd_ += plaintextLength;
  rawStart_ = recordEnd;
  ProcessPlaintext();
}

void SecureChannelServer::Connection::ProcessPlaintext() {
  while (state_ != State::kClosed && state_ != State::kDraining) {
    if (plaintextToSkip_ > 0) {
      const size_t skipped = std::min(plaintextToSkip_, plaintextEnd_ - plaintextStart_);
      plaintextStart_ += skipped;
      plaintextToSkip_ -= skipped;
    }
    if (plaintextStart_ == plaintextEnd_) { return; }
    TlvHeader header;
    const uint8_t* tlv = buffer_ + plaintextStart_;
    const size_t available = plaintextEnd_ - plaintextStart_;
    const TlvHeaderStatus status = ParseTlvHeader(tlv, available, kSecureChannelMaxLength, &header);
    if (status == TlvHeaderStatus::kNeedMoreData) { return; }
    if (status == TlvHeaderStatus::kInvalid) {
      Fail("sent a TLV whose Length is too large");
      return;
    }
    // ParseTlvHeader() limited the Length, so it fits in a size_t.
    const size_t tlvLength = header.headerSize + static_cast<size_t>(header.length);
    const bool authenticated = state_ == State::kAuthenticated;
    SecureChannelHandler* handler = nullptr;
    if (header.type == kRecordServerReady) {
      Fail("sent a ready message in the wrong direction");
      return;
    } else if (header.type == kRecordClientReady) {
      if (authenticated) {
        Fail("sent a second ClientReady");
        return;
      }
    } else if (header.type != kRecordClose) {
      if (authenticated) { handler = server_->FindHandler(header.type); }
      if (handler == nullptr) {
        plaintextToSkip_ = tlvLength;
        continue;
      }
    }
    if (available < tlvLength) { return; }
    const uint8_t* value = tlv + header.headerSize;
    plaintextStart_ += tlvLength;
    if (header.type == kRecordClientReady) {
      if (!HandleClientReady(value, header.length)) {
        Fail("failed authentication");
        return;
      }
    } else if (header.type == kRecordClose) {
      HandleClose(value, header.length);
    } else {
      handler->OnTlv(*this, header.type, value, header.length);
    }
  }
}

bool SecureChannelServer::Connection::HandleClientHello(const uint8_t* tlv, const TlvHeader& header) {
  TlvField fields[] = {
      {0, TlvField::Kind::kBytes, /*required=*/true, kSecureChannelPublicKeySize}
  };
  if (!ParseTlvFields(tlv + header.headerSize, header.length, fields, 1)) { return false; }
  ScopedP256 p256;
  ScopedPoint clientPublic;
  ScopedPoint serverPublic;
  ScopedMpi serverPrivate;
  ScopedMpi sharedSecret;
  if (!p256.loaded || !ReadPublicKey(p256, fields[0].value, &clientPublic.point)) { return false; }
  if (mbedtls_ecp_gen_keypair(&p256.group, &serverPrivate.mpi, &serverPublic.point, RandomCallback, nullptr) != 0) {
    return false;
  }
  uint8_t encodedPublic[kSecureChannelPublicKeySize];
  size_t encodedLength = 0;
  if (mbedtls_ecp_point_write_binary(&p256.group, &serverPublic.point, MBEDTLS_ECP_PF_UNCOMPRESSED, &encodedLength,
                                     encodedPublic, sizeof(encodedPublic)) != 0 ||
      encodedLength != sizeof(encodedPublic)) {
    return false;
  }
  uint8_t serverHelloValue[VarintSize(0) + VarintSize(kSecureChannelPublicKeySize) + kSecureChannelPublicKeySize];
  TlvWriter valueWriter(serverHelloValue, sizeof(serverHelloValue));
  valueWriter.WriteBytes(0, encodedPublic, sizeof(encodedPublic));
  uint8_t
      serverHello[VarintSize(kTopLevelServerHello) + VarintSize(sizeof(serverHelloValue)) + sizeof(serverHelloValue)];
  TlvWriter helloWriter(serverHello, sizeof(serverHello));
  helloWriter.WriteBytes(kTopLevelServerHello, serverHelloValue, valueWriter.size());
  if (valueWriter.failed() || helloWriter.failed() || !WriteAll(serverHello, helloWriter.size())) { return false; }

  uint8_t sharedSecretBytes[kCoordinateSize];
  uint8_t keyMaterial[kKeyMaterialSize];
  sendKeys_ = new (std::nothrow) RecordKeys();
  receiveKeys_ = new (std::nothrow) RecordKeys();
  signedDataHash_ = new (std::nothrow) mbedtls_sha256_context();
  if (signedDataHash_ != nullptr) { mbedtls_sha256_init(signedDataHash_); }
  // ProcessRaw() limited the Length, so it fits in a size_t.
  const size_t clientHelloLength = header.headerSize + static_cast<size_t>(header.length);
  const bool success =
      sendKeys_ != nullptr && receiveKeys_ != nullptr && signedDataHash_ != nullptr &&
      mbedtls_ecdh_compute_shared(&p256.group, &sharedSecret.mpi, &clientPublic.point, &serverPrivate.mpi,
                                  RandomCallback, nullptr) == 0 &&
      mbedtls_mpi_write_binary(&sharedSecret.mpi, sharedSecretBytes, sizeof(sharedSecretBytes)) == 0 &&
      DeriveKeyMaterial(sharedSecretBytes, tlv, clientHelloLength, serverHello, helloWriter.size(), keyMaterial) &&
      receiveKeys_->Set(keyMaterial, keyMaterial + kGcmKeySize) &&
      sendKeys_->Set(keyMaterial + kGcmKeySize + kGcmIvSize, keyMaterial + 2 * kGcmKeySize + kGcmIvSize) &&
      mbedtls_sha256_starts(signedDataHash_, /*is224=*/0) == 0 &&
      mbedtls_sha256_update(signedDataHash_, reinterpret_cast<const uint8_t*>(kClientReadyLabel),
                            sizeof(kClientReadyLabel)) == 0 &&
      mbedtls_sha256_update(signedDataHash_, tlv, clientHelloLength) == 0 &&
      mbedtls_sha256_update(signedDataHash_, serverHello, helloWriter.size()) == 0;
  mbedtls_platform_zeroize(sharedSecretBytes, sizeof(sharedSecretBytes));
  mbedtls_platform_zeroize(keyMaterial, sizeof(keyMaterial));
  // The private key and shared secret MPIs get zeroized when they're freed.
  if (!success) { return false; }
  state_ = State::kAwaitingClientReady;
  return true;
}

bool SecureChannelServer::Connection::HandleClientReady(const uint8_t* value, uint64_t length) {
  TlvField fields[] = {
      {0, TlvField::Kind::kBytes, /*required=*/true, kSecureChannelPublicKeySize},
      {1, TlvField::Kind::kBytes, /*required=*/true,              kSignatureSize}
  };
  if (!ParseTlvFields(value, length, fields, 2)) { return false; }
  if (!server_->IsAllowedKey(fields[0].value)) { return false; }
  uint8_t hash[kHashSize];
  // This TLV is inside a Value that's in memory, so its length fits in a size_t.
  if (mbedtls_sha256_update(signedDataHash_, fields[0].tlv, static_cast<size_t>(fields[0].tlvLength)) != 0 ||
      mbedtls_sha256_finish(signedDataHash_, hash) != 0) {
    return false;
  }
  DeleteSignedDataHash();
  ScopedP256 p256;
  ScopedPoint clientPublic;
  ScopedMpi r;
  ScopedMpi s;
  if (!p256.loaded || !ReadPublicKey(p256, fields[0].value, &clientPublic.point) ||
      mbedtls_mpi_read_binary(&r.mpi, fields[1].value, kCoordinateSize) != 0 ||
      mbedtls_mpi_read_binary(&s.mpi, fields[1].value + kCoordinateSize, kCoordinateSize) != 0 ||
      mbedtls_ecdsa_verify(&p256.group, hash, sizeof(hash), &clientPublic.point, &r.mpi, &s.mpi) != 0) {
    return false;
  }
  state_ = State::kAuthenticated;
  jll_info("SecureChannel connection %d authenticated", socket_);
  // This must precede anything else we send, and any handling of the TLVs that follow ClientReady.
  return SendTlv(kRecordServerReady, nullptr, 0);
}

void SecureChannelServer::Connection::HandleClose(const uint8_t* value, uint64_t length) {
  TlvField fields[] = {
      {0, TlvField::Kind::kInteger, /*required=*/true}
  };
  const bool valid = ParseTlvFields(value, length, fields, 1);
  // Nothing can follow a Close. We can't know what else is on its way, but we can tell if it's already here.
  const bool clean = valid && fields[0].integer == 0 && plaintextStart_ == plaintextEnd_ && rawStart_ == rawEnd_;
  jll_info("SecureChannel connection %d got Close with status %llu", socket_,
           static_cast<unsigned long long>(fields[0].integer));
  EndSession(clean);
  CloseSocket();
  state_ = State::kClosed;
}

bool SecureChannelServer::Connection::WriteAll(const uint8_t* data, size_t length) {
  size_t written = 0;
  while (written < length) {
    const ssize_t result = send(socket_, data + written, length - written, /*flags=*/0);
    if (result > 0) {
      written += static_cast<size_t>(result);
      continue;
    }
    if (result < 0 && errno != EWOULDBLOCK) { return false; }
    struct pollfd pollFd = {.fd = socket_, .events = POLLOUT, .revents = 0};
    if (poll(&pollFd, 1, kSendTimeoutMs) <= 0) { return false; }
  }
  return true;
}

// Encrypts and sends the plaintext that's in the send buffer at kRecordHeaderMaxSize.
bool SecureChannelServer::Connection::SendRecord(size_t plaintextLength) {
  uint8_t* sendBuffer = server_->sendBuffer_;
  uint8_t* plaintext = sendBuffer + kRecordHeaderMaxSize;
  const size_t valueLength = plaintextLength + kGcmTagSize;
  const size_t headerSize = VarintSize(kTopLevelRecord) + VarintSize(valueLength);
  uint8_t* header = plaintext - headerSize;
  WriteVarint(valueLength, header + WriteVarint(kTopLevelRecord, header));
  uint8_t nonce[kGcmIvSize];
  if (!sendKeys_->NextNonce(nonce) ||
      mbedtls_gcm_crypt_and_tag(&sendKeys_->gcm, MBEDTLS_GCM_ENCRYPT, plaintextLength, nonce, sizeof(nonce), header,
                                headerSize, plaintext, plaintext, kGcmTagSize, plaintext + plaintextLength) != 0) {
    return false;
  }
  return WriteAll(header, headerSize + valueLength);
}

bool SecureChannelServer::Connection::SendTlv(uint64_t type, const uint8_t* value, uint64_t length) {
  // Callers make sure that this is at most kSecureChannelMaxLength, so it fits in a size_t.
  const size_t valueSize = static_cast<size_t>(length);
  uint8_t* plaintext = server_->sendBuffer_ + kRecordHeaderMaxSize;
  size_t buffered = WriteVarint(type, plaintext);
  buffered += WriteVarint(length, plaintext + buffered);
  size_t offset = 0;
  while (true) {
    const size_t chunk = std::min(valueSize - offset, kMaxPlaintextPerSentRecord - buffered);
    if (chunk > 0) { memcpy(plaintext + buffered, value + offset, chunk); }
    offset += chunk;
    if (!SendRecord(buffered + chunk)) { return false; }
    buffered = 0;
    if (offset == valueSize) { return true; }
  }
}

bool SecureChannelServer::Connection::Send(uint64_t type, const uint8_t* value, uint64_t length) {
  if (state_ != State::kAuthenticated) { return false; }
  if (type < kSecureChannelFirstHandlerType || type > kMaxVarint || length > kSecureChannelMaxLength) {
    jll_fatal("SecureChannel handler sent an invalid TLV of type %llu and length %llu",
              static_cast<unsigned long long>(type), static_cast<unsigned long long>(length));
  }
  if (!SendTlv(type, value, length)) {
    Fail("stopped reading what we send");
    return false;
  }
  return true;
}

void SecureChannelServer::Connection::Close(uint64_t status) {
  if (state_ != State::kAuthenticated) { return; }
  uint8_t value[VarintSize(0) + VarintSize(kMaxVarintSize) + kMaxVarintSize];
  TlvWriter writer(value, sizeof(value));
  writer.WriteInteger(0, status);
  if (writer.failed() || !SendTlv(kRecordClose, value, writer.size())) {
    Fail("stopped reading what we send");
    return;
  }
  EndSession(/*clean=*/status == 0);
  shutdown(socket_, SHUT_WR);
  drainDeadline_ = TimeMicros() + kFlushTimeout;
  state_ = State::kDraining;
}

void SecureChannelServer::RegisterHandler(uint64_t firstType, uint64_t lastType, SecureChannelHandler* handler) {
  if (started_ || numHandlers_ >= kMaxHandlers || firstType < kSecureChannelFirstHandlerType || lastType < firstType) {
    jll_fatal("SecureChannel can't register a handler for types %llu to %llu",
              static_cast<unsigned long long>(firstType), static_cast<unsigned long long>(lastType));
  }
  for (size_t i = 0; i < numHandlers_; i++) {
    if (firstType <= handlers_[i].lastType && handlers_[i].firstType <= lastType) {
      jll_fatal("SecureChannel handlers overlap on types %llu to %llu", static_cast<unsigned long long>(firstType),
                static_cast<unsigned long long>(lastType));
    }
  }
  handlers_[numHandlers_++] = HandlerRange{firstType, lastType, handler};
}

SecureChannelHandler* SecureChannelServer::FindHandler(uint64_t type) const {
  for (size_t i = 0; i < numHandlers_; i++) {
    if (type >= handlers_[i].firstType && type <= handlers_[i].lastType) { return handlers_[i].handler; }
  }
  return nullptr;
}

void SecureChannelServer::NotifySessionEnd(SecureSession& session, bool clean) {
  for (size_t i = 0; i < numHandlers_; i++) { handlers_[i].handler->OnSessionEnd(session, clean); }
}

bool SecureChannelServer::IsAllowedKey(const uint8_t* key) const {
  for (size_t i = 0; i < numKeys_; i++) {
    if (memcmp(keys_[i].data(), key, kSecureChannelPublicKeySize) == 0) { return true; }
  }
  return false;
}

void SecureChannelServer::OpenListener() {
  const int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (listener < 0) {
    jll_error("SecureChannel socket() failed with error %d: %s", errno, strerror(errno));
    return;
  }
  const int one = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in address = {};
  address.sin_family = AF_INET;
  address.sin_port = htons(kSecureChannelPort);
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(listener, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0 ||
      listen(listener, /*backlog=*/2) != 0 || fcntl(listener, F_SETFL, fcntl(listener, F_GETFL) | O_NONBLOCK) < 0) {
    jll_error("SecureChannel failed to listen with error %d: %s", errno, strerror(errno));
    close(listener);
    return;
  }
  listener_ = listener;
  jll_info("SecureChannel listening on TCP port %u", kSecureChannelPort);
}

void SecureChannelServer::CloseListener() {
  if (listener_ < 0) { return; }
  close(listener_);
  listener_ = -1;
}

void SecureChannelServer::AcceptConnection() {
  // Make room first, so that we never use more sockets than kMaxConnections and the listener, plus one to refuse.
  const auto findFreeSlot = [this]() -> Connection** {
    for (Connection*& connection : connections_) {
      if (connection == nullptr) { return &connection; }
    }
    return nullptr;
  };
  Connection** slot = findFreeSlot();
  if (slot == nullptr) {
    Connection* oldest = nullptr;
    for (Connection* connection : connections_) {
      if (connection->state() == Connection::State::kAuthenticated) { continue; }
      if (oldest == nullptr || connection->acceptTime() < oldest->acceptTime()) { oldest = connection; }
    }
    if (oldest != nullptr) {
      oldest->Abort();
      ReapConnections();
      slot = findFreeSlot();
    }
  }
  struct sockaddr_in address = {};
  socklen_t addressLength = sizeof(address);
  const int sock = accept(listener_, reinterpret_cast<struct sockaddr*>(&address), &addressLength);
  if (sock < 0) {
    if (errno != EWOULDBLOCK) {
      jll_error("SecureChannel accept() failed with error %d: %s", errno, strerror(errno));
      CloseListener();
    }
    return;
  }
  if (slot == nullptr) {
    jll_info("SecureChannel refusing connection since all %zu are authenticated", kMaxConnections);
    close(sock);
    return;
  }
  const int one = 1;
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  if (fcntl(sock, F_SETFL, fcntl(sock, F_GETFL) | O_NONBLOCK) < 0) {
    jll_error("SecureChannel failed to make socket non-blocking");
    close(sock);
    return;
  }
  Connection* connection = new (std::nothrow) Connection(this, sock);
  if (connection == nullptr) {
    jll_error("SecureChannel failed to allocate a connection");
    close(sock);
    return;
  }
  *slot = connection;
  char addressString[INET_ADDRSTRLEN] = {};
  inet_ntop(AF_INET, &address.sin_addr, addressString, sizeof(addressString));
  jll_info("SecureChannel connection %d accepted from %s:%u", sock, addressString, ntohs(address.sin_port));
}

void SecureChannelServer::CheckTimeouts() {
  const Microseconds now = TimeMicros();
  for (Connection* connection : connections_) {
    if (connection != nullptr) { connection->CheckTimeout(now); }
  }
}

void SecureChannelServer::ReapConnections() {
  for (Connection*& connection : connections_) {
    if (connection == nullptr) { continue; }
    bool clean = false;
    if (connection->TakeSessionEnd(&clean)) { NotifySessionEnd(*connection, clean); }
    if (connection->state() == Connection::State::kClosed) {
      delete connection;
      connection = nullptr;
    }
  }
}

void SecureChannelServer::CloseAllSessionsAndFlush() {
  for (Connection* connection : connections_) {
    if (connection == nullptr) { continue; }
    if (connection->state() == Connection::State::kAuthenticated) {
      connection->Close(/*status=*/0);
    } else if (connection->IsHandshaking()) {
      connection->Abort();
    }
  }
  const Microseconds deadline = TimeMicros() + kFlushTimeout;
  while (TimeMicros() < deadline) {
    struct pollfd pollFds[kMaxConnections];
    nfds_t numPollFds = 0;
    for (Connection* connection : connections_) {
      if (connection == nullptr || connection->state() != Connection::State::kDraining) { continue; }
      if (connection->DiscardWhileDraining()) {
        pollFds[numPollFds++] = pollfd{.fd = connection->socket(), .events = POLLIN, .revents = 0};
      }
    }
    if (numPollFds == 0) { return; }
    poll(pollFds, numPollFds, /*timeoutMs=*/100);
  }
}

void SecureChannelServer::RunTask() {
  if (listener_ < 0) {
    OpenListener();
    if (listener_ < 0) {
      vTaskDelay(pdMS_TO_TICKS(kPollTimeoutMs));
      return;
    }
  }
  struct pollfd pollFds[1 + kMaxConnections];
  Connection* polledConnections[1 + kMaxConnections] = {};
  nfds_t numPollFds = 0;
  pollFds[numPollFds++] = pollfd{.fd = listener_, .events = POLLIN, .revents = 0};
  for (Connection* connection : connections_) {
    if (connection == nullptr) { continue; }
    polledConnections[numPollFds] = connection;
    pollFds[numPollFds++] = pollfd{.fd = connection->socket(), .events = POLLIN, .revents = 0};
  }
  if (poll(pollFds, numPollFds, kPollTimeoutMs) < 0) {
    jll_error("SecureChannel poll() failed with error %d: %s", errno, strerror(errno));
    vTaskDelay(pdMS_TO_TICKS(kPollTimeoutMs));
    return;
  }
  for (nfds_t i = 1; i < numPollFds; i++) {
    if (pollFds[i].revents != 0) { polledConnections[i]->HandleReadable(); }
  }
  CheckTimeouts();
  ReapConnections();
  if (pollFds[0].revents != 0) { AcceptConnection(); }
  // Without this, the idle task could starve and trigger the watchdog while clients keep us busy.
  vTaskDelay(1);
}

// static
void SecureChannelServer::TaskFunction(void* parameters) {
  SecureChannelServer* server = reinterpret_cast<SecureChannelServer*>(parameters);
  while (true) { server->RunTask(); }
}

void SecureChannelServer::Start(const SecureChannelPublicKey* keys, size_t numKeys) {
  if (started_) { return; }
  started_ = true;
  if (numKeys == 0) {
    jll_info("SecureChannel not starting because this firmware has no client keys");
    return;
  }
  keys_ = keys;
  numKeys_ = numKeys;
  sendBuffer_ = static_cast<uint8_t*>(malloc(kSendBufferSize));
  if (sendBuffer_ == nullptr) { jll_fatal("SecureChannel failed to allocate its send buffer"); }
  if (xTaskCreatePinnedToCore(TaskFunction, "JL_SecChannel", kTaskStackSize, /*parameters=*/this, kTaskPriority,
                              /*taskHandle=*/nullptr, kTaskCore) != pdPASS) {
    jll_fatal("Failed to create SecureChannel task");
  }
}

// static
SecureChannelServer* SecureChannelServer::Get() {
  static SecureChannelServer sSingleton;
  return &sSingleton;
}

}  // namespace jazzlights

#endif  // JL_SECURE_CHANNEL
