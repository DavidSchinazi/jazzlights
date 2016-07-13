#ifndef ARDUINO
#include "dfsparks/networks/udpsocket.h"
#include "dfsparks/log.h"
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

namespace dfsparks {

void UdpSocketNetwork::doConnection() {
  if (status() == Network::connecting) {
    int optval, flags;
    sockaddr_in if_addr;
    struct ip_mreq mc_group;

    assert(fd == 0); // must be true if status == connecting

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

    flags = fcntl(fd, F_GETFL) | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
      error("can't set socket to nonblocking mode: %s", strerror(errno));
      goto err; // Can't set socket to nonblocking mode
    }

    memset((char *)&if_addr, 0, sizeof(if_addr));
    if_addr.sin_family = AF_INET;
    if_addr.sin_port = htons((unsigned short)udp_port);
    if_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&if_addr, sizeof(if_addr)) < 0) {
      error("can't bind client socket to port %d: %s", udp_port, strerror(errno));
      goto err; // Can't bind socket
    }

    mc_group.imr_multiaddr.s_addr = inet_addr(multicast_addr);
    mc_group.imr_interface.s_addr =
        htonl(INADDR_ANY); // inet_addr("203.106.93.94");
    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc_group,
                   sizeof(mc_group)) < 0) {
      error("can't join multicast group %s: %s", multicast_addr, strerror(errno));
      goto err;
    }

    info("Connected, bound to port %d, multicast group %s", udp_port, multicast_addr);
    setStatus(Network::connected);    
  }
  else if (status() == Network::disconnecting) {
    close(fd);
    fd = 0;
    info("socket disconnected");
    setStatus(Network::disconnected);
  }
  return;

err:
  if (fd) {
    close(fd);
    fd = 0;
  }
  error("connection failed");
  setStatus(Network::connection_failed);
}

int UdpSocketNetwork::doReceive(void *buf, size_t bufsize) {
  if (!fd) {
    return -1;
  }

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

int UdpSocketNetwork::doBroadcast(void *buf, size_t bufsize) {
  if (!fd) {
    return -1;
  }

  sockaddr_in addr;
  memset((char *)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)udp_port);
  addr.sin_addr.s_addr = inet_addr(multicast_addr);
  if (sendto(fd, buf, bufsize, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    error("can't broadcast %d bytes on port %d, socket %d: %s", bufsize,
          udp_port, fd, strerror(errno));
    return -1;
  }
  return bufsize;
}

} // namespace dfsparks
#endif // ARDUINO
