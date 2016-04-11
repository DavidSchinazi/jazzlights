// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//

#include "utils.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

uint64_t GetCurrentMillis() {
  struct timespec time;
  if (clock_gettime(CLOCK_MONOTONIC, &time) == -1) {
    REPORT_ERRNO("clock_gettime(monotonic)");
    CHECK(false);
  }
  return ((uint64_t) time.tv_sec) * 1000 + time.tv_nsec / 1000000;
}

void Sleep(double seconds) {
  struct timespec req;
  struct timespec rem;
  req.tv_sec = (int) seconds;
  req.tv_nsec = (long) ((seconds - req.tv_sec) * 1000000000.0);
  rem.tv_sec = 0;
  rem.tv_nsec = 0;
  while (nanosleep(&req, &rem) == -1) {
    if (errno != EINTR) {
      REPORT_ERRNO("nanosleep");
      break;
    }
    req = rem;
  }
}

void AddTimeMillis(struct timespec* time, uint64_t increment) {
  uint64_t nsec = (increment % 1000000) * 1000000 + time->tv_nsec;
  time->tv_sec += increment / 1000 + nsec / 1000000000;
  time->tv_nsec = nsec % 1000000000;
}

Bytes::Bytes(const void* data, int len)
    : data_(NULL), len_(0) {
  SetData(data, len);
}

Bytes::~Bytes() {
  Clear();
}

void Bytes::Clear() {
  if (data_)
    delete[] data_;
  data_ = NULL;
  len_ = 0;
}

void Bytes::SetData(const void* data, int len) {
  Clear();
  data_ = new uint8_t[len];
  memcpy(data_, data, len);
  len_ = len;
}

void Bytes::MoveOwnership(void* data, int len) {
  Clear();
  data_ = reinterpret_cast<uint8_t*>(data);
  len_ = len;
}
