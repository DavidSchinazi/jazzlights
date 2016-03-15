// DiscoFish wearables protocol
#ifndef DFWP_PROTOCOL_H
#define DFWP_PROTOCOL_H

enum { 
   DFWP_FRAME_NONE,
   DFWP_FRAME_SOLID,
   DFWP_FRAME_RANDOM
};

struct dfwp_header {
   uint16_t protocol_version;
   uint32_t frame_type;
   uint32_t frame_size;
} __attribute__ ((__packed__));

struct dfwp_frame_solid_data {
   uint32_t color;
} __attribute__ ((__packed__));

struct dfwp_frame {
   dfwp_header header;
   union {
      dfwp_frame_solid_data solid;    
   } data;
};

#define DISCOFISH_WEARABLES_PROTO_VERSION 0
#define DISCOFISH_WEARABLES_PORT 0xDF0D
#define DISCOFISH_WEARABLES_FRAME_SIZE sizeof(dfwp_frame)


#endif /* DFWP_PROTOCOL_H */