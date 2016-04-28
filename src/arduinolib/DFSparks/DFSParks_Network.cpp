#include "DFSparks_Network.h"
#include "DFSparks_Timer.h"
#include <stdint.h>
#include <string.h>
#ifndef ARDUINO
#include <arpa/inet.h>
#else
constexpr uint32_t ntohl(uint32_t n) {
  return ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) | ((n & 0xFF0000) >> 8) |
         ((n & 0xFF000000) >> 24);
}

constexpr uint32_t htonl(uint32_t n) {
  return ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) | ((n & 0xFF0000) >> 8) |
         ((n & 0xFF000000) >> 24);
}
#endif

namespace dfsparks {

constexpr int udp_port = 0xDF0D;
constexpr size_t max_message_size = 256;

enum class MessageType { UNKNOWN = 0, PATTERN = 0xDF0001 };

struct MessageHeader {
  int32_t type;
  int32_t size;
} __attribute__((__packed__));

struct PatternMessage {
  MessageHeader header;
  int32_t pattern;
  int32_t elapsed;
} __attribute__((__packed__));

static_assert(sizeof(PatternMessage) < max_message_size,
              "PatternMessage is larger than max message size");

static int decode_pattern(void *buf, size_t bufsize, int *pattern,
                          int32_t *elapsed_ms) {
  *pattern = -1;
  *elapsed_ms = -1;
  if (bufsize < sizeof(PatternMessage)) {
    return -1;
  }
  PatternMessage &m = *static_cast<PatternMessage *>(buf);
  if (ntohl(m.header.type) != static_cast<int32_t>(MessageType::PATTERN)) {
    return -1;
  }
  *pattern = ntohl(m.pattern);
  *elapsed_ms = ntohl(m.elapsed);
  return sizeof(PatternMessage);
}

static int encode_pattern(void *buf, size_t bufsize, int pattern,
                          int32_t elapsed_ms) {
  if (bufsize < sizeof(PatternMessage)) {
    return -1;
  }
  PatternMessage &m = *static_cast<PatternMessage *>(buf);
  m.header.type = htonl(static_cast<int32_t>(MessageType::PATTERN));
  m.header.size = htonl(sizeof(m));
  m.pattern = htonl(static_cast<int32_t>(pattern));
  m.elapsed = htonl(elapsed_ms);
  return sizeof(PatternMessage);
}

void Client::begin() {
  running = false;
  effect_id = -1;
  effect_start_time = -1;
  received_time = -1;
  if (on_begin(udp_port) >= 0) {
    running = true;
  }
}

void Client::update() {
  int32_t curr_time = time_ms();
  if (running) {
    char buf[max_message_size];
    int32_t elapsed_time;
    int received_bytes = on_recv(buf, sizeof(buf));
    if (received_bytes > 0 &&
        decode_pattern(buf, received_bytes, &effect_id, &elapsed_time) > 0)
      received_time = curr_time;
      effect_start_time = time_ms() - elapsed_time;
  }

  if (curr_time - received_time > timeout) {
    effect_id = -1;
    effect_start_time = -1;
  }
}

void Server::begin() { running = on_begin() >= 0; }

void Server::update(int effect_id, int32_t elapsed_time) {
  if (!running) {
    return;
  }
  int32_t curr_time = time_ms();
  if (sent_effect_id == effect_id && sent_time > 0 &&
      curr_time - sent_time < interval) {
    return; /* nothing new to broadcast */
  }
  char buf[max_message_size];
  int encoded_bytes = encode_pattern(buf, sizeof(buf), effect_id, elapsed_time);
  if (encoded_bytes > 0 &&
      on_send("255.255.255.255", udp_port, buf, encoded_bytes) > 0) {
    sent_effect_id = effect_id;
    sent_time = curr_time;
  }
}

} // namespace dfsparks
