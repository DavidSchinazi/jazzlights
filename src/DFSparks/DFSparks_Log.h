#ifndef DFSPARKS_LOG_H
#define DFSPARKS_LOG_H
#include <stdarg.h>

namespace dfsparks {
  void log(const char *level, const char *fmt, va_list args );

  inline void info(const char *fmt, ... ) { 
    va_list args;
    va_start(args, fmt);
    log("INFO", fmt, args);
    va_end(args);
  }
  
  inline void error(const char *fmt, ... ) { 
    va_list args;
    va_start(args, fmt);
    log("ERROR", fmt, args);
    va_end(args);
  }
} 
#endif /* DFSPARKS_LOG_H */