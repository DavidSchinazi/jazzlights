#ifndef DFSPARKS_PROTOCOL_H
#define DFSPARKS_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>

namespace dfsparks {
   namespace frame {
      const int max_size = 256;
      
      enum class Type {
         UNKNOWN, SOLID, RANDOM
      };

      Type decode_type(void *buf, size_t bufsize);

      namespace solid {
         size_t encode(void *buf, size_t bufsize, uint32_t color);
         size_t decode(void *buf, size_t bufsize, uint32_t *color);
      } // namespace solid

   } // namespace frame

const int proto_version = 0;
const int udp_port = 0xDF0D;

} // namespace dfsparks

#endif /* DFSPARKS_PROTOCOL_H */