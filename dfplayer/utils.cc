// Copyright 2014, Igor Chernyshev.
// Licensed under Apache 2.0 ASF license.
//

#include "utils.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

//#include <csignal>

uint64_t GetCurrentMillis() {
  struct timespec time;
  if (clock_gettime(CLOCK_MONOTONIC, &time) == -1) {
    REPORT_ERRNO("clock_gettime(monotonic)");
    CHECK(false);
  }
  return ((uint64_t) time.tv_sec) * 1000 + time.tv_nsec / 1000000;
}

Bytes::Bytes(void* data, int len)
    : data_(reinterpret_cast<uint8_t*>(data)), len_(len) {}

Bytes::~Bytes() {
  Clear();
}

void Bytes::Clear() {
  if (data_)
    delete[] data_;
  data_ = NULL;
  len_ = 0;
}

void Bytes::SetData(void* data, int len) {
  Clear();
  data_ = reinterpret_cast<uint8_t*>(data);
  len_ = len;
}

