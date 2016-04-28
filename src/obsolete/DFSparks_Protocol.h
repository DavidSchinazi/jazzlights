#ifndef DFSPARKS_PROTOCOL_H
#define DFSPARKS_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#ifndef ARDUINO
# include <arpa/inet.h>
#else 
   inline uint32_t ntohl(uint32_t n) {
      return ((n & 0xFF) << 24)  | 
            ((n & 0xFF00) << 8) |
            ((n & 0xFF0000) >> 8) |
            ((n & 0xFF000000) >> 24);
   }

   inline uint32_t htonl(uint32_t n) {
      return ((n & 0xFF) << 24) | 
            ((n & 0xFF00) << 8) | 
               ((n & 0xFF0000) >> 8) | 
               ((n & 0xFF000000) >> 24);
   }
#endif

namespace dfsparks {
   constexpr const char *proto_name = "DFSP";
   constexpr uint8_t proto_version_major = 1;
   constexpr uint8_t proto_version_minor = 0;
   constexpr int udp_port = 0xDF0D;
} // namespace dfsparks

#endif /* DFSPARKS_PROTOCOL_H */