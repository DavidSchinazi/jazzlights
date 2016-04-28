#ifndef DFSPARKS_UTIL_H
#define DFSPARKS_UTIL_H
#include <stdint.h>

namespace dfsparks {

   int pack(void *buf, size_t bufsize, const char *fmt, ...);
   int unpack(void *buf, size_t bufsize, const char *fmt, ...);

}


#endif /* DFSPARKS_UTIL_H */