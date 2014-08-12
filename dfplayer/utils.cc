// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//

#include "utils.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <errno.h>
#include <math.h>
#include <opencv2/opencv.hpp>
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

void Bytes::SetData(void* data, int len) {
  Clear();
  data_ = new uint8_t[len];
  memcpy(data_, data, len);
  len_ = len;
}

// Resizes image using bilinear interpolation.
uint8_t* ResizeImage(
    uint8_t* src, int src_w, int src_h, int dst_w, int dst_h) {
  cv::Mat src_img(src_h, src_w, CV_8UC4, src);
  cv::Mat dst_img(cv::Size(dst_w, dst_h), CV_8UC4);
  cv::resize(src_img, dst_img, dst_img.size());
  dst_img = dst_img.clone();  // Make it contiguous
  int dst_len = dst_w * dst_h * 4;
  uint8_t* dst = new uint8_t[dst_len];
  memcpy(dst, dst_img.data, dst_len);
  return dst;
}

uint8_t* FlipImage(uint8_t* src, int w, int h, bool horizontal) {
  cv::Mat src_img(h, w, CV_8UC4, src);
  cv::Mat dst_img(cv::Size(w, h), CV_8UC4);
  cv::flip(src_img, dst_img, (horizontal ? 1 : 0));
  int dst_len = w * h * 4;
  uint8_t* dst = new uint8_t[dst_len];
  memcpy(dst, dst_img.data, dst_len);
  return dst;
}

#define BLEND_COLOR(f, b, a)                               \
    (uint8_t) ( ( ((uint32_t) (a)) * (f) +                 \
                  (255 - ((uint32_t) (a))) * (b) ) / 255 )

void PasteSubImage(
    uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_x, int dst_y, int dst_w, int dst_h,
    bool enable_alpha) {
  for (int y = 0; y < src_h; y++) {
    int y2 = dst_y + y;
    if (y2 >= dst_h)
      break;
    for (int x = 0; x < src_w; x++) {
      int x2 = dst_x + x;
      if (x2 >= dst_w)
        break;
      int src_pos = (y * src_w + x) * 4;
      int dst_pos = (y2 * dst_w + x2) * 4;
      uint8_t alpha = src[src_pos + 3];
      if (!enable_alpha || alpha == 255) {
        COPY_PIXEL(dst, dst_pos, src, src_pos);
      } else if (alpha != 0) {
        dst[dst_pos] = BLEND_COLOR(src[src_pos], dst[dst_pos], alpha);
        dst[dst_pos + 1] = BLEND_COLOR(
            src[src_pos + 1], dst[dst_pos + 1], alpha);
        dst[dst_pos + 2] = BLEND_COLOR(
            src[src_pos + 2], dst[dst_pos + 2], alpha);
      }
    }
  }
}

