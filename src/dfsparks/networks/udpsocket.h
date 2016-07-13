#ifndef ARDUINO
#ifndef DFSPARKS_NETWORK_UDPSOCKET_H
#define DFSPARKS_NETWORK_UDPSOCKET_H
#include "dfsparks/network.h"

namespace dfsparks {

class UdpSocketNetwork : public Network {
  void doConnection() final;
  int doReceive(void *buf, size_t bufsize) final;
  int doBroadcast(void *buf, size_t bufsize) final;

  int fd;
};

} // namespace dfsparks

#endif /* DFSPARKS_NETWORK_UDPSOCKET_H */
#endif /* !ARDUINO */