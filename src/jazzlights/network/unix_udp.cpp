#include "jazzlights/network/unix_udp.h"

#ifndef ESP32

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef __APPLE__
#include <net/if_dl.h>
#elif defined(linux) || defined(__linux) || defined(__linux__)
#include <linux/if_packet.h>
#endif  // __APPLE__

#include "jazzlights/util/log.h"

namespace jazzlights {

// returns 2 when newly created, 1 when already existing, 0 on failure.
int UnixUdpNetwork::setupSocketForInterface(const char* ifName, struct in_addr localAddr) {
  int fd = -1;
  auto search = sockets_.find(std::string(ifName));
  if (search != sockets_.end()) { fd = search->second; }
  if (fd >= 0) {
    // We already have a valid socket for this interface.
    return 1;
  }

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  do {
    if (fd < 0) {
      jll_error("Failed to create UDP socket");
      break;
    }

    int one = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) < 0) {
      jll_error("Failed to set reuseaddr option on UDP socket %d ifName %s: %s", fd, ifName, strerror(errno));
      break;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) < 0) {
      jll_error("Failed to set reuseport option on UDP socket %d ifName %s: %s", fd, ifName, strerror(errno));
      break;
    }

    int flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
      jll_error("Failed to set UDP socket %d ifName %s to nonblocking mode: %s", fd, ifName, strerror(errno));
      break;
    }

    sockaddr_in sin = {
        .sin_family = AF_INET,
        .sin_port = htons(port_),
        // .sin_addr = localAddr,
        .sin_addr = {htonl(INADDR_ANY)},
        .sin_zero = {},
    };

    if (bind(fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
      jll_error("Failed to bind UDP socket %d ifName %s to port %d: %s", fd, ifName, port_, strerror(errno));
      break;
    }

    struct ip_mreq mcastGroup = {
        .imr_multiaddr = mcastAddr_,
        .imr_interface = localAddr,
    };
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mcastGroup, sizeof(mcastGroup)) < 0) {
      jll_error("Failed to add UDP socket %d ifName %s to multicast group %s: %s", fd, ifName, mcastAddrStr_,
                strerror(errno));
      break;
    }

    // Disable receiving our own multicast traffic.
    u_char zero = 0;
    if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &zero, sizeof(zero)) < 0) {
      jll_error("Failed to disable multicast loopack on UDP socket %d ifName %s: %s", fd, ifName, strerror(errno));
      break;
    }

    sockets_[ifName] = fd;

    jll_info("Joined multicast group %s, listening on port %d, UDP socket %d, ifName %s", mcastAddrStr_, port_, fd,
             ifName);
    return 2;
  } while (false);

  // Error handling.
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
  jll_error("UDP socket connection failed");
  return 0;
}

void UnixUdpNetwork::invalidateSocket(std::string ifName) {
  auto search = sockets_.find(ifName);
  if (search == sockets_.end()) {
    // No socket for ifName.
    jll_error("Did not find socket to invalidate for ifName %s", ifName.c_str());
    return;
  }
  int fd = search->second;
  if (fd >= 0) { close(fd); }
  sockets_.erase(ifName);
  jll_info("Invalidated socket %d for ifName %s", fd, ifName.c_str());
}

bool UnixUdpNetwork::setupSockets() {
  struct ifaddrs* ifaddr = NULL;
  if (getifaddrs(&ifaddr) == -1) {
    jll_error("getifaddrs failed: %s", strerror(errno));
    return false;
  }

  uint32_t newValidSockets = 0;
  for (struct ifaddrs* ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      jll_error("Skipping interface without data \"%s\"", ifa->ifa_name);
      continue;
    }
    if (localDeviceId_ == NetworkDeviceId()) {
      // Fill in localDeviceId_.
#if defined(__APPLE__)
      if (ifa->ifa_addr->sa_family != AF_LINK) { continue; }
      struct sockaddr_dl* dlAddress = reinterpret_cast<struct sockaddr_dl*>(ifa->ifa_addr);
      NetworkDeviceId localAddress(reinterpret_cast<const uint8_t*>(&dlAddress->sdl_data[dlAddress->sdl_nlen]));
#elif defined(linux) || defined(__linux) || defined(__linux__)
      if (ifa->ifa_addr->sa_family != AF_PACKET) { continue; }
      NetworkDeviceId localAddress((reinterpret_cast<struct sockaddr_ll*>(ifa->ifa_addr)->sll_addr));
#else
#error "Unsupported platform"
#endif
      if (localAddress != NetworkDeviceId()) {
        jll_info("Choosing local MAC address " DEVICE_ID_FMT " from interface %s", DEVICE_ID_HEX(localAddress),
                 ifa->ifa_name);
        localDeviceId_ = localAddress;
      }
    }
    if (ifa->ifa_addr->sa_family != AF_INET) {
      // We currently only support IPv4.
      continue;
    }
    if (strcmp(ifa->ifa_name, "lo0") == 0 ||  // macOS
        strcmp(ifa->ifa_name, "lo") == 0) {   // Linux
      // Do not use loopback.
      continue;
    }

    sockaddr_in* sin = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
    if (setupSocketForInterface(ifa->ifa_name, sin->sin_addr) == 2) { newValidSockets++; }
  }

  freeifaddrs(ifaddr);
  if (newValidSockets > 0) { jll_info("Created %u new valid sockets", newValidSockets); }
  return !sockets_.empty();
}

UnixUdpNetwork::UnixUdpNetwork(uint16_t port, const char* addr) : port_(port) {
  assert(strnlen(addr, sizeof(mcastAddrStr_) + 1) < sizeof(mcastAddrStr_));
  strncpy(mcastAddrStr_, addr, sizeof(mcastAddrStr_));
  int parsed = inet_pton(AF_INET, addr, &mcastAddr_);
  assert(parsed == 1);
  // Make sure localDeviceId_ is filled in.
  setupSockets();
}

NetworkStatus UnixUdpNetwork::update(NetworkStatus status, Milliseconds /*currentTime*/) {
  if (status == INITIALIZING || status == CONNECTING) {
    setupSockets();
    return CONNECTED;
  } else if (status == DISCONNECTING) {
    for (auto pair : sockets_) {
      std::string ifName = pair.first;
      int fd = pair.second;
      close(fd);
      jll_info("Disconnected UDP socket %d for ifName %s", fd, ifName.c_str());
    }
    sockets_.clear();
    return DISCONNECTED;
  }

  // Nothing to do
  return status;
}

int UnixUdpNetwork::recv(void* buf, size_t bufsize, std::string* /*details*/) {
  for (auto pair : sockets_) {
    std::string ifName = pair.first;
    int fd = pair.second;
    sockaddr_in fromaddr = {};
    socklen_t fromaddrlen = sizeof(fromaddr);
    // jll_info("Attempting to read %llu bytes on UDP socket %d ifName %s",
    //      static_cast<uint64_t>(bufsize), fd, ifName.c_str());
    ssize_t n = recvfrom(fd, buf, bufsize, 0, reinterpret_cast<sockaddr*>(&fromaddr), &fromaddrlen);
    if (n < 0) {
      const int errorCode = errno;
      if (errorCode == EWOULDBLOCK || errorCode == EAGAIN) {
        continue;  // no data on nonblocking socket
      }
      jll_error("Failed to receive data on UDP socket %d ifName %s: %s", fd, ifName.c_str(), strerror(errorCode));
      invalidateSocket(ifName);
      setupSockets();
      // Exit loop since sockets_ has been modified, and that invalidates the loop iterator.
      break;
    }

    char addressString[INET_ADDRSTRLEN] = {};
    if (is_debug_logging_enabled()) {
      if (inet_ntop(AF_INET, &(fromaddr.sin_addr), addressString, sizeof(addressString)) == nullptr) {
        jll_fatal("Printing receive address failed with error %d: %s", errno, strerror(errno));
      }
    }

    jll_debug("Received %ld bytes on UDP socket %d ifName %s from %s:%d", n, fd, ifName.c_str(), addressString,
              ntohs(fromaddr.sin_port));
    return n;
  }
  return -1;
}

void UnixUdpNetwork::send(void* buf, size_t bufsize) {
  setupSockets();
  sockaddr_in sin = {
      .sin_family = AF_INET,
      .sin_port = htons(port_),
      .sin_addr = mcastAddr_,
      .sin_zero = {},
  };
  for (auto pair : sockets_) {
    std::string ifName = pair.first;
    int fd = pair.second;
    ssize_t sendResult = sendto(fd, buf, bufsize, 0, (struct sockaddr*)&sin, sizeof(sin));
    if (sendResult < 0) {
      const int errorCode = errno;
      if (errorCode == EWOULDBLOCK || errorCode == EAGAIN) {
        continue;  // no room to enqueue data on nonblocking socket
      }
      jll_error("Failed to send %zu bytes on UDP socket %d ifName %s: %s", bufsize, fd, ifName.c_str(),
                strerror(errorCode));
      if (errorCode == ENETUNREACH) {
        // Do not invalidate the socket as we would immediately
        // recreate it and infinite loop.
        continue;
      }
      invalidateSocket(ifName);
      setupSockets();
      // Exit loop since sockets_ has been modified, and that invalidates the loop iterator.
      break;
    }
    if (static_cast<size_t>(sendResult) != bufsize) {
      jll_error("Incorrectly sent %zd bytes instead of %zu on UDP socket %d ifName %s: %s", sendResult, bufsize, fd,
                ifName.c_str(), strerror(errno));
      invalidateSocket(ifName);
      setupSockets();
      // Exit loop since sockets_ has been modified, and that invalidates the loop iterator.
      break;
    }
    jll_debug("Sent %zu bytes on UDP socket %d ifName %s to %s:%d", bufsize, fd, ifName.c_str(), mcastAddrStr_, port_);
  }
}

}  // namespace jazzlights
#endif  // ESP32
