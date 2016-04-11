// Copyright 2014, Igor Chernyshev.

#include "util/pixels.h"

#include <math.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// RgbGamma
////////////////////////////////////////////////////////////////////////////////

RgbGamma::RgbGamma() {
  SetGamma(1);
}

void RgbGamma::SetGamma(double gamma) {
  SetGammaRanges(0, 255, gamma, 0, 255, gamma, 0, 255, gamma);
}

void RgbGamma::SetGammaRanges(
    int r_min, int r_max, double r_gamma,
    int g_min, int g_max, double g_gamma,
    int b_min, int b_max, double b_gamma) {
  if (r_min < 0 || r_max > 255 || r_min > r_max || r_gamma < 0.1 ||
      g_min < 0 || g_max > 255 || g_min > g_max || g_gamma < 0.1 ||
      b_min < 0 || b_max > 255 || b_min > b_max || b_gamma < 0.1) {
    // TODO(igorc): Use logging macros.
    fprintf(stderr, "Incorrect gamma values\n");
    return;
  }

  for (int i = 0; i < 256; i++) {
    double d = ((double) i) / 255.0;
    gamma_r_[i] =
        (int) (r_min + floor((r_max - r_min) * pow(d, r_gamma) + 0.5));
    gamma_g_[i] =
        (int) (g_min + floor((g_max - g_min) * pow(d, g_gamma) + 0.5));
    gamma_b_[i] =
        (int) (b_min + floor((b_max - b_min) * pow(d, b_gamma) + 0.5));
  }
}

void RgbGamma::Apply(uint8_t* dst, const uint8_t* src, int w, int h) const {
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int i = (y * w + x) * 4;
      Apply(dst + i, src + i);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// RgbaImage
////////////////////////////////////////////////////////////////////////////////

RgbaImage::RgbaImage() : data_(NULL) {
  Set(NULL, 0, 0);
}

RgbaImage::RgbaImage(uint8_t* data, int w, int h) : data_(NULL) {
  Set(data, w, h);
}

RgbaImage::RgbaImage(const RgbaImage& src) : data_(NULL) {
  Set(src.data_, src.width_, src.height_);
}

RgbaImage& RgbaImage::operator=(const RgbaImage& rhs) {
  // We could avoid memory copies... but it is not a big deal yet.
  Set(rhs.data_, rhs.width_, rhs.height_);
  return *this;
}

RgbaImage::~RgbaImage() {
  delete[] data_;
}

void RgbaImage::Set(uint8_t* data, int w, int h) {
  delete[] data_;
  data_ = NULL;
  width_ = w;
  height_ = h;
  int len = RGBA_LEN(w, h);
  if (len) {
    data_ = new uint8_t[len];
    memcpy(data_, data, len);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Standalone Functions
////////////////////////////////////////////////////////////////////////////////

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

uint8_t* RotateImage(
    uint8_t* src, int src_w, int src_h, int w, int h, int angle) {
  cv::Mat src_img(src_h, src_w, CV_8UC4, src);
  cv::Mat dst_img(cv::Size(w, h), CV_8UC4);

  int max_dim = std::max(src_img.cols, src_img.rows);
  cv::Mat r = cv::getRotationMatrix2D(
      cv::Point2f(max_dim / 2.0, max_dim / 2.0), angle, 1.0);
  cv::warpAffine(src_img, dst_img, r, cv::Size(w, h));

  int dst_len = RGBA_LEN(w, h);
  uint8_t* dst = new uint8_t[dst_len];
  memcpy(dst, dst_img.data, dst_len);
  return dst;
}

uint8_t* CropImage(
    uint8_t* src, int src_w, int src_h,
    int crop_x, int crop_y, int crop_w, int crop_h) {
  cv::Mat src_img(src_h, src_w, CV_8UC4, src);
  cv::Rect roi(crop_x, crop_y, crop_w, crop_h);
  cv::Mat dst_img = src_img(roi);
  int dst_len = RGBA_LEN(crop_w, crop_h);
  uint8_t* dst = new uint8_t[dst_len];
  memcpy(dst, dst_img.clone().data, dst_len);
  return dst;
}

#define BLEND_COLOR(f, b, a)                               \
    (uint8_t) ( ( ((uint32_t) (a)) * (f) +                 \
                  (255 - ((uint32_t) (a))) * (b) ) / 255 )

void PasteSubImage(
    uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_x, int dst_y, int dst_w, int dst_h,
    bool enable_alpha, bool carry_alpha) {
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
        if (carry_alpha)  // TODO(igorc): Remove this param.
          dst[dst_pos + 3] = src[src_pos + 3];
      }
    }
  }
}

void EraseAlpha(uint8_t* img, int w, int h) {
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int pos = (y * w + x) * 4;
      img[pos + 3] = 255;
    }
  }
}
