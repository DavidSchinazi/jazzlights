// Copyright 2014, Igor Chernyshev.
// Licensed under Apache 2.0 ASF license.
//

#ifndef __DFPLAYER_UTILS_H
#define __DFPLAYER_UTILS_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

#define COPY_PIXEL(dst, dst_pos, src, src_pos)                  \
  *((uint32_t*) ((uint8_t*) (dst) + (dst_pos))) =               \
      *((uint32_t*)((uint8_t*) (src) + (src_pos)))

#define PIX_LEN(w, h)   ((w) * (h) * 4)

class Autolock {
 public:
  Autolock(pthread_mutex_t& lock) : lock_(&lock) {
    int err = pthread_mutex_lock(lock_);
    if (err != 0) {
      fprintf(stderr, "Unable to aquire mutex: %d\n", err);
      CHECK(false);
    }
  }

  ~Autolock() {
    int err = pthread_mutex_unlock(lock_);
    if (err != 0) {
      fprintf(stderr, "Unable to release mutex: %d\n", err);
      CHECK(false);
    }
  }

 private:
  Autolock(const Autolock& src);
  Autolock& operator=(const Autolock& rhs);

  pthread_mutex_t* lock_;
};

// Contains array of bytes and deallocates in destructor.
struct Bytes {
  Bytes(void* data, int len);
  ~Bytes();

  void SetData(void* data, int len);
  uint8_t* GetData() const { return data_; }
  int GetLen() const { return len_; }

 private:
  Bytes(const Bytes& src);
  Bytes& operator=(const Bytes& rhs);
  void Clear();
  uint8_t* data_;
  int len_;
};

uint64_t GetCurrentMillis();

// Resizes image using bilinear interpolation.
uint8_t* ResizeImage(
    uint8_t* src, int src_w, int src_h, int dst_w, int dst_h);

uint8_t* FlipImage(uint8_t* src, int w, int h);

void PasteSubImage(
    uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_x, int dst_y, int dst_w, int dst_h);

#endif  // __DFPLAYER_UTILS_H

