// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//

#ifndef __DFPLAYER_UTILS_H
#define __DFPLAYER_UTILS_H

#include <stdint.h>

// Contains array of bytes and deallocates in destructor.
struct Bytes {
  Bytes() : data_(nullptr), len_(0) {}
  Bytes(const void* data, int len);
  ~Bytes();

  void SetData(const void* data, int len);
  void MoveOwnership(void* data, int len);
  uint8_t* GetData() const { return data_; }
  int GetLen() const { return len_; }

 private:
  Bytes(const Bytes& src);
  Bytes& operator=(const Bytes& rhs);
  void Clear();
  uint8_t* data_;
  int len_;
};

#endif  // __DFPLAYER_UTILS_H

