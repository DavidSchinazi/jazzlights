#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

#include "socket.h"

namespace dfsparks {

// ====================================================================
// Socket
// ====================================================================

sockaddr_in mkaddr(const char *host, int port) {
  sockaddr_in addr;
  memset((char *)&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons((unsigned short)port);
  if (!strcmp(host, "*")) {
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
  } else {
    addr.sin_addr.s_addr = inet_addr(host);
  }
  return addr;
}

Socket::Socket() {
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    throw std::system_error(errno, std::system_category());
  }
}

Socket::~Socket() { close(fd); }

Socket &Socket::set_reuseaddr(bool v) {
  int optval = v;
  return setopt(SO_REUSEADDR, &optval, sizeof(optval));
}

Socket &Socket::set_broadcast(bool v) {
  int optval = v;
  return setopt(SO_BROADCAST, &optval, sizeof(optval));
}

Socket &Socket::set_nonblocking(bool v) {
  int flags = fcntl(fd, F_GETFL);
  if (v) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }
  if (fcntl(fd, F_SETFL, flags) < 0) {
    throw std::system_error(errno, std::system_category(),
                            "Can't set socket to nonblocking mode");
  }
  return *this;
}

Socket &Socket::set_rcvtimeout(uint64_t millis) {
  struct timeval tv;
  tv.tv_sec = millis / 1000;
  tv.tv_usec = (millis % 1000) * 1000;
  return setopt(SO_RCVTIMEO, (char *)&tv, sizeof(tv));
}

Socket &Socket::setopt(int option_name, const void *val, size_t valsize) {
  if (::setsockopt(fd, SOL_SOCKET, option_name, val, valsize) < 0) {
    throw std::system_error(errno, std::system_category(),
                            "Can't set socket option");
  }
  return *this;
}

void Socket::bind(const char *host, int port) {
  sockaddr_in addr = mkaddr(host, port);
  if (::bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    std::stringstream msg;
    msg << "Can't bind socket to " << host << ":" << port;
    throw std::system_error(errno, std::system_category(), msg.str());
  }
}

void Socket::sendto(const char *host, int port, void *data, size_t len) {
  sockaddr_in clientaddr = mkaddr(host, port);
  if (::sendto(fd, data, len, 0, (struct sockaddr *)&clientaddr,
               sizeof(clientaddr)) < 0) {
    std::stringstream msg;
    msg << "Can't send " << len << " bytes to " << host << ":" << port;
    throw std::system_error(errno, std::system_category(), msg.str());
  }
}

size_t Socket::recvfrom(void *buf, size_t len, int flags) {
  sockaddr_in serveraddr;
  socklen_t serverlen = sizeof(serveraddr);
  int n = ::recvfrom(fd, buf, len, flags,
                     reinterpret_cast<sockaddr *>(&serveraddr), &serverlen);
  if (n < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return 0; // no data on nonblocking socket
    }
    throw std::system_error(errno, std::system_category());
  }
  return n;
}

} // namespace dfsparks
