#include "unisparks/network.h"
#include "unisparks/log.h"
#include "unisparks/timer.h"

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

namespace unisparks {

const char* const multicast_addr = "239.255.223.01";

struct Message {
  int32_t msgcode;
  char reserved[12];
  struct Frame {
    char pattern[16];
    int32_t elapsed_ms;
    int32_t beat_ms;
    uint8_t hue_med;
    uint8_t hue_dev;
  } __attribute__((__packed__)) frame;  
} __attribute__((__packed__));

bool Network::broadcast(const NetworkFrame& fr) {
  doConnection();
  if (status() != connected) {
    return false;
  }
  Message p;
  info("TX %d bytes, frame %-16s elapsed:%010d beat:%010d", sizeof(p),
         fr.pattern, fr.timeElapsed, fr.timeSinceBeat);
  p.msgcode = htonl(msg_frame);
  strncpy(p.frame.pattern, fr.pattern, sizeof(p.frame.pattern));
  p.frame.elapsed_ms = htonl(fr.timeElapsed);
  p.frame.beat_ms = htonl(fr.timeSinceBeat);
  lastTxTime_ = timeMillis();
  return doBroadcast(&p, sizeof(p)) == sizeof(p);
}

bool Network::poll() {
  doConnection();
  if (status() != connected) {
    return false;
  }
  Message p;
  size_t n = doReceive(&p, sizeof(p));
  if (n == 0) {
    return false;
  }
  // TODO: Fix size!
  if (n < sizeof(p)) {
    error("RX packet too short, received %d bytes, expected at least %d bytes", 
      n, sizeof(p));
    return false;
  }
  p.msgcode = ntohl(p.msgcode);
  if (p.msgcode != msg_frame) {
    error("RX unknown message, code: %d", p.msgcode);
    return false;
  }
  lastRxTime_ = timeMillis();
  NetworkFrame fr = {p.frame.pattern,
      static_cast<int32_t>(ntohl(p.frame.elapsed_ms)),
      static_cast<int32_t>(ntohl(p.frame.beat_ms))};
  info("RX %d bytes, frame %-16s elapsed:%010d beat:%010d", n, fr.pattern, 
    fr.timeElapsed, fr.timeSinceBeat);
  NetworkListener *l = first_;
  while(l) {
    l->onReceived(*this, fr);
    l = l->next_;
  }
  return true;
}

void Network::add(NetworkListener& l) {
  l.next_ = first_;
  first_ = &l;
}

void Network::remove(NetworkListener& l) {
  NetworkListener *pr = nullptr;
  NetworkListener *p = first_;
  while(p && p != &l) {
    pr = p;
    p = p->next_;
  }
  if (p && pr) {
    pr->next_ = p->next_;
  }
  else if (p) {
    first_ = p->next_;
  }
}

void Network::disconnect() {
  if (status_ == connected) {
    setStatus(disconnecting);
    poll();
  }
}

void Network::reconnect() {
  if (status_ != connected) {
    setStatus(connecting);
    poll();
  }
}

void Network::setStatus(Status s) {
  if (s != status_) {
    status_ = s;
    NetworkListener *l = first_;
    while(l) {
      l->onStatusChange(*this);
      l = l->next_;
    }    
  }
}

} // namespace unisparks

