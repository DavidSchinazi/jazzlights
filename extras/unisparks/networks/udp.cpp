#ifndef ARDUINO
#include "unisparks/networks/udp.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "unisparks/util/log.hpp"

namespace unisparks {

bool Udp::setupSockets() {
  int optval, flags;
  sockaddr_in if_addr;
  struct ip_mreq mc_group;

  assert(fd_ < 0); // must be true if status == connecting

  fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  do {
    if (fd_ < 0) {
      error("Can't create client UDP socket");
      break;
    }

    optval = 1;
    if (setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
      error("Can't set reuseaddr option on UDP socket %d: %s", fd_, strerror(errno));
      break;
    }

    flags = fcntl(fd_, F_GETFL) | O_NONBLOCK;
    if (fcntl(fd_, F_SETFL, flags) < 0) {
      error("Can't set UDP socket %d to nonblocking mode: %s", fd_, strerror(errno));
      break;
    }

    memset((char*)&if_addr, 0, sizeof(if_addr));
    if_addr.sin_family = AF_INET;
    if_addr.sin_port = htons(port_);
    if_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd_, (struct sockaddr*)&if_addr, sizeof(if_addr)) < 0) {
      error("Can't bind UDP socket %d to port %d: %s", fd_, port_, strerror(errno));
      break;
    }

    mc_group.imr_multiaddr = mcastAddr_;
    mc_group.imr_interface.s_addr =
      htonl(INADDR_ANY); // inet_addr("203.106.93.94");
    if (setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc_group,
                   sizeof(mc_group)) < 0) {
      error("Can't add UDP socket %d to multicast group %s: %s", fd_, mcastAddrStr_,
            strerror(errno));
      break;
    }

    info("Joined multicast group %s, listening on port %d, UDP socket %d",
         mcastAddrStr_, port_, fd_);
    return true;
  } while (false);

  // Error handling.
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
  error("UDP socket connection failed");
  return false;
}

Udp::Udp(int p, const char* addr) : port_(static_cast<uint16_t>(p)), fd_(-1) {
  assert(strnlen(addr, sizeof(mcastAddrStr_) + 1) < sizeof(mcastAddrStr_));
  strncpy(mcastAddrStr_, addr, sizeof(mcastAddrStr_));
  int parsed = inet_aton(addr, &mcastAddr_);
  assert(parsed == 1);
}

NetworkStatus Udp::update(NetworkStatus status) {
  if (status == INITIALIZING || status == CONNECTING) {
    bool success = setupSockets();
    return success ? CONNECTED : CONNECTION_FAILED;
  } else if (status == DISCONNECTING) {
    close(fd_);
    info("Disconnected UDP socket %d", fd_);
    fd_ = -1;
    return DISCONNECTED;
  }

  // Nothing to do
  return status;
}

int Udp::recv(void* buf, size_t bufsize) {
  if (fd_ < 0) {
    return -1;
  }

  const char* fromstr = nullptr;
  sockaddr_in fromaddr;
  socklen_t fromaddrlen = sizeof(fromaddr);
  int n = recvfrom(fd_, buf, bufsize, 0,
                   reinterpret_cast<sockaddr*>(&fromaddr), &fromaddrlen);
  if (n < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return 0; // no data on nonblocking socket
    }
    error("Can't receive data on UDP socket %d: %s", fd_, strerror(errno));
    return -1; // error reading
  }
  fromstr = inet_ntoa(fromaddr.sin_addr);
  info("Received %d bytes on UDP socket %d from %s:%d", n, fd_, fromstr,
        ntohs(fromaddr.sin_port));
  return n;
}

void Udp::send(void* buf, size_t bufsize) {
  if (fd_ < 0) {
    return; // -1;
  }

  sockaddr_in addr;
  memset((char*)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr = mcastAddr_;
  if (static_cast<size_t>(sendto(fd_, buf, bufsize, 0, (struct sockaddr*)&addr,
                                 sizeof(addr))) != bufsize) {
    error("Can't send %d bytes on UDP socket %d: %s", bufsize, fd_,
          strerror(errno));
    return; // -1;
  }
  info("Sent %d bytes on UDP socket %d to %s:%d", bufsize, fd_, mcastAddrStr_, port_);
  //return bufsize;
}

} // namespace unisparks
#endif // ARDUINO
