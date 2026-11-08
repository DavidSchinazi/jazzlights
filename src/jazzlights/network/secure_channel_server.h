#ifndef JL_NETWORK_SECURE_CHANNEL_SERVER_H
#define JL_NETWORK_SECURE_CHANNEL_SERVER_H

#include "jazzlights/util/config.h"

#if JL_SECURE_CHANNEL

#include <array>
#include <cstddef>
#include <cstdint>

namespace jazzlights {

// Implements the server side of the protocol described in extras/docs/OTA.md: it authenticates clients, sets
// up session keys and carries encrypted TLVs, and it knows nothing about what those TLVs are for. Each use of the
// channel implements SecureChannelHandler and registers it for its range of types.

inline constexpr uint16_t kSecureChannelPort = 6699;
inline constexpr size_t kSecureChannelPublicKeySize = 65;
// The first type that's available to uses of the channel. Lower types are reserved for the channel itself.
inline constexpr uint64_t kSecureChannelFirstHandlerType = 3;

// The maximum Length of a ClientHello. That of any other top-level TLV, including records, and of any TLV in the stream
// that records carry, is kSecureChannelMaxLength. These keep what a connection buffers small enough for the device.
inline constexpr uint64_t kSecureChannelMaxClientHelloLength = 8192;
inline constexpr uint64_t kSecureChannelMaxLength = 4096;

// The Close status for a handler that received a malformed TLV.
inline constexpr uint64_t kSecureChannelStatusMalformed = 1;

using SecureChannelPublicKey = std::array<uint8_t, kSecureChannelPublicKeySize>;

// One authenticated session. Must only be used from the channel's task, which is where handlers run.
class SecureSession {
 public:
  // Sends one TLV, whose type must be at least kSecureChannelFirstHandlerType and whose length must be at most
  // kSecureChannelMaxLength. Returns false if the session has ended,
  // or if the client stopped reading, which ends the session.
  virtual bool Send(uint64_t type, const uint8_t* value, uint64_t length) = 0;
  // Sends a Close with this status and ends the session. A status of 0 means no error.
  virtual void Close(uint64_t status) = 0;

 protected:
  virtual ~SecureSession() = default;
};

class SecureChannelHandler {
 public:
  virtual ~SecureChannelHandler() = default;
  // Called on the channel's task, shared by all sessions, so this must not block for long. `value` holds the whole
  // Value, however many records it arrived in.
  virtual void OnTlv(SecureSession& session, uint64_t type, const uint8_t* value, uint64_t length) = 0;
  // Called for every session that was authenticated, once it has ended. `clean` is false if the session failed or the
  // connection was lost.
  virtual void OnSessionEnd(SecureSession& session, bool clean) = 0;
};

class SecureChannelServer {
 public:
  static SecureChannelServer* Get();

  SecureChannelServer(const SecureChannelServer&) = delete;
  SecureChannelServer& operator=(const SecureChannelServer&) = delete;

  // Registers a handler for types from `firstType` to `lastType` inclusive. Types with no handler are skipped. Must be
  // called before Start.
  void RegisterHandler(uint64_t firstType, uint64_t lastType, SecureChannelHandler* handler);

  // Starts the channel's task, which listens on kSecureChannelPort. The keys aren't copied and must outlive the server.
  // This does nothing if there are no keys, since nobody could authenticate. The network stack must be initialized.
  void Start(const SecureChannelPublicKey* keys, size_t numKeys);

  // Sends a Close with status 0 on every authenticated session, closes the other connections, and waits up to a second
  // for what was sent to reach the clients. Must only be called from a handler, and is meant for use right before
  // rebooting: no more handlers run on any session afterwards.
  void CloseAllSessionsAndFlush();

 private:
  class Connection;
  static constexpr size_t kMaxHandlers = 4;
  // Each connection can have up to a TCP window of 5744 bytes waiting in lwIP, which takes about 7KB of heap that
  // the channel can't limit, so this keeps that to about 28KB.
  static constexpr size_t kMaxConnections = 4;
  struct HandlerRange {
    uint64_t firstType;
    uint64_t lastType;
    SecureChannelHandler* handler;
  };

  SecureChannelServer() = default;
  static void TaskFunction(void* parameters);
  void RunTask();
  void OpenListener();
  void CloseListener();
  void AcceptConnection();
  void CheckTimeouts();
  void ReapConnections();
  SecureChannelHandler* FindHandler(uint64_t type) const;
  void NotifySessionEnd(SecureSession& session, bool clean);
  bool IsAllowedKey(const uint8_t* key) const;

  HandlerRange handlers_[kMaxHandlers] = {};
  size_t numHandlers_ = 0;
  const SecureChannelPublicKey* keys_ = nullptr;
  size_t numKeys_ = 0;
  bool started_ = false;
  int listener_ = -1;
  Connection* connections_[kMaxConnections] = {};
  uint8_t* sendBuffer_ = nullptr;
};

}  // namespace jazzlights

#endif  // JL_SECURE_CHANNEL
#endif  // JL_NETWORK_SECURE_CHANNEL_SERVER_H
