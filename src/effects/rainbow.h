// Copyright 2016, Igor Chernyshev.

#ifndef EFFECTS_RAINBOW_H_
#define EFFECTS_RAINBOW_H_

#include <opencv2/opencv.hpp>

#include "model/effect.h"
#include "util/pixels.h"

class RainbowEffect : public Effect {
 public:
  RainbowEffect(int x);
  ~RainbowEffect() override;

  void SetX(int x);

  void Apply(RgbaImage* dst, bool* is_done) override;

  void Destroy() override;

 protected:
  void DoInitialize() override;

 private:
  RainbowEffect(const RainbowEffect& src);
  RainbowEffect& operator=(const RainbowEffect& rhs);

  static const int kRainbowSize = 256;

  int rainbow_target_x_ = -1;
  int rainbow_effective_x_ = -1;
  int rainbow_height_ = 0;
  int rainbow_shift_ = kRainbowSize - 1;
  int rainbow_cycle_ = 0;
  cv::Mat rainbow_;
};

#endif  // EFFECTS_RAINBOW_H_
