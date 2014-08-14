// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//

#ifndef __DFPLAYER_UTILS_H
#define __DFPLAYER_UTILS_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

#define RGBA_LEN(w, h)   ((w) * (h) * 4)

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

struct RgbGamma {
  RgbGamma();

  void SetGamma(double gamma);
  void SetGammaRanges(
      int r_min, int r_max, double r_gamma,
      int g_min, int g_max, double g_gamma,
      int b_min, int b_max, double b_gamma);

  void Apply(uint8_t* dst, const uint8_t* src, int w, int h) const;

 private:
  double gamma_r_[256];
  double gamma_g_[256];
  double gamma_b_[256];
};

struct RgbaImage {
  RgbaImage();
  RgbaImage(uint8_t* data, int w, int h);
  RgbaImage(const RgbaImage& src);
  RgbaImage& operator=(const RgbaImage& rhs);
  ~RgbaImage();

  void Clear() { Set(NULL, 0, 0); }
  void Set(uint8_t* data, int w, int h);

  bool IsEmpty() const { return data_ == NULL; }
  uint8_t* GetData() const { return data_; }
  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  int GetDataLen() const { return RGBA_LEN(width_, height_); }

 private:
  uint8_t* data_;
  int width_;
  int height_;
};

struct Coord {
  Coord(int x, int y) : x_(x), y_(y) {}

  int x_;
  int y_;
};

uint64_t GetCurrentMillis();

void Sleep(double seconds);

void AddTimeMillis(struct timespec* time, uint64_t increment);

// Resizes image using bilinear interpolation.
uint8_t* ResizeImage(
    uint8_t* src, int src_w, int src_h, int dst_w, int dst_h);

uint8_t* FlipImage(uint8_t* src, int w, int h, bool horizontal);

void PasteSubImage(
    uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_x, int dst_y, int dst_w, int dst_h,
    bool enable_alpha);

#endif  // __DFPLAYER_UTILS_H

