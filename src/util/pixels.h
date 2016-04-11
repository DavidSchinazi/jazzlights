// Copyright 2016, Igor Chernyshev.

#ifndef UTIL_PIXEL_H_
#define UTIL_PIXEL_H_

#include <stdint.h>
#include <unistd.h>

#define COPY_PIXEL(dst, dst_pos, src, src_pos)                  \
  *((uint32_t*) ((uint8_t*) (dst) + (dst_pos))) =               \
      *((uint32_t*)((uint8_t*) (src) + (src_pos)))

#define RGBA_LEN(w, h)   ((w) * (h) * 4)

#define PACK_COLOR32(r, g, b, a)    \
  (((a) << 24) | ((b) << 16) | ((g) << 8) | (r))

struct RgbGamma {
  RgbGamma();

  void SetGamma(double gamma);
  void SetGammaRanges(
      int r_min, int r_max, double r_gamma,
      int g_min, int g_max, double g_gamma,
      int b_min, int b_max, double b_gamma);

  inline uint32_t Apply(uint32_t color) const {
    uint32_t r = gamma_r_[color & 0xFF];
    uint32_t g = gamma_g_[(color >> 8) & 0xFF];
    uint32_t b = gamma_b_[(color >> 16) & 0xFF];
    return PACK_COLOR32(r, g, b, (color >> 24));
  }

  inline void Apply(uint8_t* dst, const uint8_t* src) const {
    *((uint32_t*) dst) = Apply(*((uint32_t*) src));
  }

  void Apply(uint8_t* dst, const uint8_t* src, int w, int h) const;

 private:
  double gamma_r_[256];
  double gamma_g_[256];
  double gamma_b_[256];
};

// TODO(igorc): Reduce the number of data copies.
// Maybe reuse OpenCV classes.
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

// Resizes image using bilinear interpolation.
uint8_t* ResizeImage(
    uint8_t* src, int src_w, int src_h, int dst_w, int dst_h);

uint8_t* FlipImage(uint8_t* src, int w, int h, bool horizontal);

uint8_t* RotateImage(
    uint8_t* src, int src_w, int src_h, int w, int h, int angle);

uint8_t* CropImage(
    uint8_t* src, int src_w, int src_h,
    int crop_x, int crop_y, int crop_w, int crop_h);

void PasteSubImage(
    uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_x, int dst_y, int dst_w, int dst_h,
    bool enable_alpha, bool carry_alpha);

void EraseAlpha(uint8_t* img, int w, int h);

#endif  // UTIL_PIXEL_H_
