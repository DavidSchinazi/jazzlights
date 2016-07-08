#include "dfsparks/network.h"
#include "dfsparks/player.h"
#include "dfsparks/timer.h"

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
constexpr int32_t msgcode = 0xDF0002; /* for verification and versioning */

struct PlayerStateMessage {
  int32_t msgcode;
  char effect_name[16];
  int32_t elapsed_ms;
  int32_t since_beat_ms;
} __attribute__((__packed__));

static int decode(void *buf, size_t bufsize, uint32_t curr_time,
                  PlayerState *pst) {
  if (bufsize < sizeof(PlayerStateMessage)) {
    error("buffer too short for decoding message: %d < %d", bufsize,
          sizeof(PlayerStateMessage));
    return -1;
  }
  PlayerStateMessage &m = *static_cast<PlayerStateMessage *>(buf);
  if (ntohl(m.msgcode) != msgcode) {
    error("can't decode unknown msgcode: %d", ntohl(m.msgcode));
    return -1;
  }
  m.effect_name[sizeof(m.effect_name) - 1] = '\0'; // ensure terminator

  PlayerState &st = *pst;
  st.effect_name = m.effect_name;
  st.start_time = curr_time - ntohl(m.elapsed_ms);
  st.beat_time = int32_t(ntohl(m.since_beat_ms)) < 0
                     ? -1
                     : curr_time - ntohl(m.since_beat_ms);
  return sizeof(PlayerStateMessage);
}

static int encode(void *buf, size_t bufsize, uint32_t curr_time,
                  const PlayerState &st) {
  if (bufsize < sizeof(PlayerStateMessage)) {
    error("buffer too short for encoding message: %d < %d", bufsize,
          sizeof(PlayerStateMessage));
    return -1;
  }
  PlayerStateMessage &m = *static_cast<PlayerStateMessage *>(buf);
  m.msgcode = htonl(msgcode);
  if (strlen(st.effect_name) >= sizeof(m.effect_name)) {
    error("effect name too long for encoding: %s >= %d chars long",
          st.effect_name, sizeof(m.effect_name));
    return -1;
  }
  strcpy(m.effect_name, st.effect_name);
  m.elapsed_ms = htonl(curr_time - st.start_time);
  m.since_beat_ms = htonl(st.beat_time < 0 ? -1 : curr_time - st.beat_time);
  return sizeof(PlayerStateMessage);
}

// ==========================================================================
//  Network
// ==========================================================================
void Network::begin() { on_begin(udp_port); }

void Network::end() { on_end(); }

void Network::broadcast(const PlayerState &state) {
  int32_t curr_time = time_ms();
  if (sent_time < 0 || curr_time - sent_time > send_interval) {
    // info("snd %s started:%d beat:%d at %d", state.effect_name,
    // state.start_time, state.beat_time, curr_time);
    int encoded_bytes = encode(buf, sizeof(buf), curr_time, state);
    if (encoded_bytes > 0 && on_broadcast(udp_port, buf, encoded_bytes) > 0) {
      sent_time = curr_time;
    }
  }
}

int Network::sync(PlayerState *state) {
  int32_t curr_time = time_ms();
  if (on_recv(buf, sizeof(buf)) > 0 &&
      decode(buf, sizeof(buf), curr_time, &received_state)) {
    received_time = curr_time;
    info("rcv %s started:%d beat:%d at %d", received_state.effect_name,
         received_state.start_time, received_state.beat_time, received_time);
  }
  if (received_time > 0 && curr_time - received_time < recv_timeout) {
    *state = received_state;
    return 0;
  }
  return -1;
}

#ifndef ARDUINO

int UdpSocketNetwork::on_begin(int port) {
  int optval, flags;
  sockaddr_in addr;

  if (fd) {
    return 0; // already initialized
  }

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    error("can't create client socket");
    goto err; // Can't create socket
  }

  optval = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
    error("can't set reuseaddr option on socket: %s", strerror(errno));
    goto err;
  }

  optval = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0) {
    error("can't set broadcast option on socket: %s", strerror(errno));
    goto err;
  }

  flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) {
    error("can't set socket to nonblocking mode: %s", strerror(errno));
    goto err; // Can't set socket to nonblocking mode
  }

  memset((char *)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    error("can't bind client socket to port %d: %s", port, strerror(errno));
    goto err; // Can't bind socket
  }
  info("bound to port %d", port);
  return 0;

err:
  if (fd) {
    close(fd);
    fd = 0;
  }
  return -1;
}

int UdpSocketNetwork::on_end() {
  close(fd);
  fd = 0;
  return 0;
}

int UdpSocketNetwork::on_recv(char *buf, size_t bufsize) {
  sockaddr_in serveraddr;
  socklen_t serverlen = sizeof(serveraddr);
  int n = recvfrom(fd, buf, bufsize, 0,
                   reinterpret_cast<sockaddr *>(&serveraddr), &serverlen);
  if (n < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return 0; // no data on nonblocking socket
    }
    error("can't receive data from socket %d: %s", fd, strerror(errno));
    goto err; // error reading
  }
  return n;
err:
  return -1;
}

int UdpSocketNetwork::on_broadcast(int port, char *buf, size_t bufsize) {
  sockaddr_in addr;
  memset((char *)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = inet_addr("255.255.255.255");
  if (sendto(fd, buf, bufsize, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    error("can't broadcast %d bytes on port %d, socket %d: %s", bufsize, port,
          fd, strerror(errno));
    return -1;
  }
  return bufsize;
}

Network &get_udp_socket_network() {
  static UdpSocketNetwork inst;
  inst.begin();
  return inst;
}
#endif // ARDUINO

} // namespace dfsparks

