#include <stdint.h>
#include <stdarg.h>
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
#endif /* ARDUINO */
#include "DFSparks_Util.h"

namespace dfsparks {

template<typename T>
int pack_v(va_list argp, void *buf, size_t bufsize) {
    T v = va_arg(argp, T);


}

int pack(void *buf, size_t bufsize, const char *fmt, ...) {
  va_list argp;
  va_start(argp, fmt);
  int packed = 0;
  for(char *p = fmt; *p; ++p) {
    switch(*p) {
      case '!': {
      }
      break;
      
      case 'B': {
        unsigned char v = va_arg(argp, unsigned char);
        *buf 
      } 
      break;
      
      default: 
        return packed;
    }  
  }
  va_end(argp);
  return packed;
}


int unpack(void *buf, size_t bufsize, const char *fmt, ...) {
  return 0;
}



} // namespace dfsparks

