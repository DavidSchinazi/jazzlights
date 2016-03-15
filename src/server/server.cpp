#include <system_error>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
 
#include "server/server.h"
#include "DFSparks.h"

namespace dfsparks {

Server::Server() : sockfd(-1) {
  int portno = udp_port;
  int err = 0;
  int optval = 1;
  struct sockaddr_in serveraddr;

  // open socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockfd < 0) {
    goto fail;
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
             (const void *)&optval , sizeof(optval)) < 0) {
    goto fail;
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST,
             (const void *)&optval , sizeof(optval)) < 0) {
    goto fail;
  }

  // build server's address
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  // build client's address
  bzero((char *) &clientaddr, sizeof(clientaddr));
  clientaddr.sin_family = AF_INET;
  clientaddr.sin_addr.s_addr = inet_addr("255.255.255.255");
  clientaddr.sin_port = htons((unsigned short)portno);

  // bind to port
  if(bind(sockfd, (struct sockaddr *) &serveraddr,
          sizeof(serveraddr)) < 0) {
    goto fail;
  }

  return;

fail:
  err = errno;
  close(sockfd);
  throw std::system_error(err, std::system_category(), "Can't create server");
}

Server::~Server() {
  close(sockfd);
}

void Server::broadcast_solid_color(uint32_t color) {
  using frame::Frame;
  using frame::Type;

  Frame frame;
  size_t size = sizeof(frame.header) + sizeof(frame.data);
  memset(&frame, 0, sizeof(frame));
  frame.header.frame_type = htonl(static_cast<uint32_t>(Type::SOLID));
  frame.header.frame_size = htonl(size);
  frame.data.solid.color = htonl(color);

  if (sendto(sockfd, &frame, size, 0,
               (struct sockaddr *)&clientaddr, sizeof(clientaddr)) < 0) {
    throw std::system_error(errno, std::system_category(), "Can't broadcast frame");
  }
}

} // namespace dfsparks
