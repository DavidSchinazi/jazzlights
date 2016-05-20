// Copyright 2016, Igor Chernyshev.

#ifndef MODEL_IMAGE_SOURCE_H_
#define MODEL_IMAGE_SOURCE_H_

#include <memory>

#include "util/pixels.h"

class ImageSource {
 public:
  ImageSource(int width, int height, int fps);
  virtual ~ImageSource() = default;

  virtual std::unique_ptr<RgbaImage> GetImage(int frame_id) = 0;

 protected:
  int width() const { return width_; }
  int height() const { return height_; }
  int fps() const { return fps_; }

 private:
  const int width_;
  const int height_;
  const int fps_;
};

#endif  // MODEL_IMAGE_SOURCE_H_
