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
