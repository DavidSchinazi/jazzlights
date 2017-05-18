#ifndef ARDUINO
#ifndef UNISPARKS_NETWORK_UDPSOCKET_H
#define UNISPARKS_NETWORK_UDPSOCKET_H
#include "unisparks/network.h"

namespace unisparks {

class UdpSocketNetwork : public Network {
  void doConnection() final;
  int doReceive(void *buf, size_t bufsize) final;
  int doBroadcast(void *buf, size_t bufsize) final;

  int fd;
};

} // namespace unisparks

#endif /* UNISPARKS_NETWORK_UDPSOCKET_H */
#endif /* !ARDUINO */