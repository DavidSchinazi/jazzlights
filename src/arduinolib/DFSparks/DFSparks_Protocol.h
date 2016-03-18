#ifndef DFSPARKS_PROTOCOL_H
#define DFSPARKS_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>

namespace dfsparks {
   namespace frame {
      const int max_size = 256;
      
      enum class Type {
         UNKNOWN, SOLID, RANDOM, RAINBOW
      };

      Type decode_type(void *buf, size_t bufsize);
 
      namespace solid {
         size_t encode(void *buf, size_t bufsize, uint32_t color);
         size_t decode(void *buf, size_t bufsize, uint32_t *color);
      } // namespace solid

      namespace random {
         size_t encode(void *buf, size_t bufsize, uint32_t sequence);
         size_t decode(void *buf, size_t bufsize, uint32_t *sequence);
      } // namespace random

      namespace rainbow {
         size_t encode(void *buf, size_t bufsize, double from_hue, double to_hue, double saturation, double lightness);
         size_t decode(void *buf, size_t bufsize, double *from_hue, double *to_hue, double *saturation, double *lightness);
      } // namespace random

   } // namespace frame

   constexpr const char *proto_name = "DFSP";
   constexpr uint8_t proto_version_major = 1;
   constexpr uint8_t proto_version_minor = 0;
   constexpr int udp_port = 0xDF0D;

} // namespace dfsparks

#endif /* DFSPARKS_PROTOCOL_H */