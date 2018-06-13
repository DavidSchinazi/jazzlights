#ifndef ARDUINO
#include "unisparks/networks/udp.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
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


NetworkStatus Udp::update(NetworkStatus status) {
  if (status == INITIALIZING || status == CONNECTING) {
    int optval, flags;
    sockaddr_in if_addr;
    struct ip_mreq mc_group;

    assert(fd == 0); // must be true if status == connecting

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
      error("Can't create client UDP socket");
      goto err;
    }

    optval = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
      error("Can't set reuseaddr option on UDP socket %d: %s", fd, strerror(errno));
      goto err;
    }

    flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
      error("Can't set UDP socket %d to nonblocking mode: %s", fd, strerror(errno));
      goto err;
    }

    memset((char*)&if_addr, 0, sizeof(if_addr));
    if_addr.sin_family = AF_INET;
    if_addr.sin_port = htons((unsigned short)port);
    if_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr*)&if_addr, sizeof(if_addr)) < 0) {
      error("Can't bind UDP socket %d to port %d: %s", fd, port, strerror(errno));
      goto err;
    }

    mc_group.imr_multiaddr.s_addr = inet_addr(mcastAddr);
    mc_group.imr_interface.s_addr =
      htonl(INADDR_ANY); // inet_addr("203.106.93.94");
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc_group,
                   sizeof(mc_group)) < 0) {
      error("Can't add UDP socket %d to multicast group %s: %s", fd, mcastAddr,
            strerror(errno));
      goto err;
    }

    info("Joined multicast group %s, listening on port %d, UDP socket %d",
         mcastAddr, port, fd);
    return CONNECTED;
  } else if (status == DISCONNECTING) {
    close(fd);
    info("Disconnected UDP socket %d", fd);
    fd = 0;
    return DISCONNECTED;
  }

  // Nothing to do
  return status;

err:
  if (fd) {
    close(fd);
    fd = 0;
  }
  error("UDP socket connection failed");
  return CONNECTION_FAILED;
}

int Udp::recv(void* buf, size_t bufsize) {
  if (!fd) {
    return -1;
  }

  const char* fromstr = nullptr;
  sockaddr_in fromaddr;
  socklen_t fromaddrlen = sizeof(fromaddr);
  int n = recvfrom(fd, buf, bufsize, 0,
                   reinterpret_cast<sockaddr*>(&fromaddr), &fromaddrlen);
  if (n < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return 0; // no data on nonblocking socket
    }
    error("Can't receive data on UDP socket %d: %s", fd, strerror(errno));
    goto err; // error reading
  }
  fromstr = inet_ntoa(fromaddr.sin_addr);
  debug("Received %d bytes on UDP socket %d from %s:%d", n, fd, fromstr,
        fromaddr.sin_port);
  return n;
err:
  return -1;
}

void Udp::send(void* buf, size_t bufsize) {
  if (!fd) {
    return; // -1;
  }

  sockaddr_in addr;
  memset((char*)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  addr.sin_addr.s_addr = inet_addr(mcastAddr);
  if (static_cast<size_t>(sendto(fd, buf, bufsize, 0, (struct sockaddr*)&addr,
                                 sizeof(addr))) != bufsize) {
    error("Can't send %d bytes on UDP socket %d: %s", bufsize, fd,
          strerror(errno));
    return; // -1;
  }
  debug("Sent %d bytes on UDP socket %d", bufsize, fd);
  //return bufsize;
}


} // namespace unisparks
#endif // ARDUINO
