// DiscoFish wearables protocol
#ifndef DFSPARKS_PROTOCOL_H
#define DFSPARKS_PROTOCOL_H

namespace dfsparks {
   namespace frame {

      enum class Type {
         NONE, SOLID, RANDOM
      };

      struct Header {
         uint16_t protocol_version;
         uint32_t frame_type;
         uint32_t frame_size;
      } __attribute__ ((__packed__));   

   struct SolidData {
      uint32_t color;
   } __attribute__ ((__packed__));

   struct Frame {
      Header header;
      union {
         SolidData solid;    
      } data;
   };

} // namespace frame

const int proto_version = 0;
const int udp_port = 0xDF0D;
const int frame_size = sizeof(frame::Frame);

} // namespace dfsparks

#endif /* DFSPARKS_PROTOCOL_H */