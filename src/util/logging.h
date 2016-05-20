// Copyright 2016, Igor Chernyshev.

#ifndef UTIL_LOGGING_H_
#define UTIL_LOGGING_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define REPORT_ERRNO(name)                           \
  fprintf(stderr, "Failure in '%s' call: %d, %s\n",  \
          name, errno, strerror(errno));

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CHECK(cond)                                             \
  if (!(cond)) {                                                \
    fprintf(stderr, "EXITING with check-fail at %s (%s:%d)"     \
            ". Condition = '" TOSTRING(cond) "'\n",             \
            __FILE__, __FUNCTION__, __LINE__);                  \
    exit(-1);                                                   \
  }

#endif  // UTIL_LOGGING_H_
