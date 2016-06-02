#include "DFSparks_Network.h"
#include "DFSparks_Timer.h"
#include <stdint.h>
#include <string.h>
#ifndef ARDUINO
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
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

// ==========================================================================
// Protocol
// ==========================================================================

constexpr int udp_port = 0xDF0D;
constexpr size_t max_message_size = 256;

enum class MessageType : uint32_t { UNKNOWN = 0, PATTERN = 0xDF0001 };

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

// ==========================================================================
//  NetworkPlayer
// ==========================================================================
void NetworkPlayer::begin(Mode m) {
  mode = m;
  if (mode == client) {
    on_start_listening(udp_port);
  } else if (mode == server) {
    on_start_serving();
  }
  Player::begin();
}

void NetworkPlayer::end() {
  Player::end();
  if (mode == client) {
    on_stop_listening();
  } else if (mode == server) {
    on_stop_serving();
  }
}

void NetworkPlayer::render() {
  int32_t curr_time = time_ms();
  switch (mode) {
  case client: {
    char buf[max_message_size];
    int32_t effect_id, elapsed_time;
    int received_bytes = on_recv(buf, sizeof(buf));
    if (received_bytes > 0 &&
        decode_pattern(buf, received_bytes, &effect_id, &elapsed_time) > 0) {
      received_time = curr_time;
      play(effect_id); // sync(effect_id, curr_time - elapsed_time, -1);
    }
  } break;
  case server: {
    int effect_id = get_playing_effect();
    if (sent_effect_id != effect_id || sent_time < 0 ||
        curr_time - sent_time < send_interval) {
      char buf[max_message_size];
      int encoded_bytes =
          encode_pattern(buf, sizeof(buf), get_playing_effect(), 0);
      if (encoded_bytes > 0 && on_broadcast(udp_port, buf, encoded_bytes) > 0) {
        sent_effect_id = effect_id;
        sent_time = curr_time;
      }
    }
  } break;
  case offline:
    /* do nothing */
    break;
  }

  Player::render();
}

#ifndef ARDUINO
// ==========================================================================
//  UdpSocketPlayer
// ==========================================================================
int UdpSocketPlayer::on_start_listening(int port) {
  int optval, flags;
  sockaddr_in addr;

  if (fd) {
    error("can't start listening: socket already opened");
    goto err; 
  }

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    error("can't create client socket");
    goto err; // Can't create socket
  }

  optval = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
    error("can't set reuseaddr option on client socket");
    goto err; 
  }

  flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) {
    error("can't set client socket to nonblocking mode");
    goto err; // Can't set socket to nonblocking mode
  }

  memset((char *)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    error("can't bind client socket to port %d", port);
    goto err; // Can't bind socket
  }
  info("bound to port %d", port);
  return 0;

err:
  if (fd) {
    close(fd);
  }
  return -1;
}

int UdpSocketPlayer::on_stop_listening() {
  close(fd);
  fd = 0;
  return 0;
}

int UdpSocketPlayer::on_recv(char *buf, size_t bufsize) {
  sockaddr_in serveraddr;
  socklen_t serverlen = sizeof(serveraddr);
  int n = recvfrom(fd, buf, bufsize, 0,
                   reinterpret_cast<sockaddr *>(&serveraddr), &serverlen);
  if (n < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return 0; // no data on nonblocking socket
    }
    error("can't receive data from client socket");
    goto err; // error reading
  }
  return n;
err:
  return -1;
}

int UdpSocketPlayer::on_start_serving() {
  int flags, optval;
  if (fd) {
    error("can't start serving: socket already opened");
    goto err; 
  }

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    error("can't create server socket");
    goto err; 
  }

  optval = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
    error("can't set broadcast option on server socket");
    goto err; 
  }

  flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) {
    error("can't set server socket to nonblocking mode");
    goto err; 
  }
  return 0;

err:
  if (fd) {
    close(fd);
  }
  return -1;
}

int UdpSocketPlayer::on_stop_serving() {
  close(fd);
  fd = 0;
  return 0;
}

int UdpSocketPlayer::on_broadcast(int port, char *buf, size_t bufsize) {
  sockaddr_in addr;
  memset((char *)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (sendto(fd, buf, bufsize, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    error("can't broadcast %d bytes on port %d", bufsize, port);
    return -1; 
  }
  return bufsize;
}
#endif // ARDUINO

} // namespace dfsparks
