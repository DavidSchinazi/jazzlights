// Copyright 2016, Igor Chernyshev.

#ifndef UTIL_PIXEL_H_
#define UTIL_PIXEL_H_

#include <stdint.h>
#include <unistd.h>

#include <memory>
#include <vector>

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
class RgbaImage {
 public:
  RgbaImage();
  RgbaImage(const uint8_t* data, int w, int h);
  RgbaImage(const RgbaImage& src);
  RgbaImage& operator=(const RgbaImage& rhs);
  ~RgbaImage();

  void Clear() { Set(NULL, 0, 0); }
  void Set(const uint8_t* data, int w, int h);
  void Set(const std::vector<uint8_t>& data, int w, int h);

  // Resizes image storage. Actual data will likely become garbage.
  void ResizeStorage(int w, int h);

  int width() const { return width_; }
  int height() const { return height_; }

  bool empty() const { return data_.empty(); }
  uint8_t* data() { return &data_[0]; }
  const uint8_t* data() const { return &data_[0]; }
  int data_size() const { return data_.size(); }

  std::unique_ptr<RgbaImage> CloneAndClear(bool null_if_empty);

 private:
  std::vector<uint8_t> data_;
  int width_;
  int height_;
};

// Resizes image using bilinear interpolation.
// TODO(igorc): Move these into RgbaImage.
uint8_t* ResizeImage(
    const uint8_t* src, int src_w, int src_h, int dst_w, int dst_h);
void ResizeImage(
    const uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_w, int dst_h);

uint8_t* FlipImage(const uint8_t* src, int w, int h, bool horizontal);

uint8_t* RotateImage(
    const uint8_t* src, int src_w, int src_h, int w, int h, int angle);

uint8_t* CropImage(
    const uint8_t* src, int src_w, int src_h,
    int crop_x, int crop_y, int crop_w, int crop_h);

void PasteSubImage(
    const uint8_t* src, int src_w, int src_h,
    uint8_t* dst, int dst_x, int dst_y, int dst_w, int dst_h,
    bool enable_alpha, bool carry_alpha);

void EraseAlpha(uint8_t* img, int w, int h);

#endif  // UTIL_PIXEL_H_
