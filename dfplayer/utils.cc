// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//

#include "utils.h"

#include <string.h>

Bytes::Bytes(const void* data, int len)
    : data_(nullptr), len_(0) {
  SetData(data, len);
}

Bytes::~Bytes() {
  Clear();
}

void Bytes::Clear() {
  if (data_)
    delete[] data_;
  data_ = nullptr;
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
