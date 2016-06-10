// Copyright 2016, Igor Chernyshev.

#include "effects/rainbow.h"

#include "util/lock.h"
#include "util/logging.h"

RainbowEffect::RainbowEffect(int x) : rainbow_target_x_(x) {
  cv::Mat greyscale;
  greyscale.create(1, kRainbowSize, CV_8UC1);
  for (int i = 0; i < kRainbowSize; ++i) {
    greyscale.at<uint8_t>(0, i) = i;
  }
  cv::Mat coloredMap;
  cv::applyColorMap(greyscale, coloredMap, cv::COLORMAP_RAINBOW);
  cv::cvtColor(coloredMap, rainbow_, CV_BGR2RGBA);
}

RainbowEffect::~RainbowEffect() {}

void RainbowEffect::DoInitialize() {}

void RainbowEffect::Destroy() {
  delete this;
}

void RainbowEffect::SetX(int x) {
  CHECK(x >= 0);
  Autolock l(lock_);
  rainbow_target_x_ = x;
}

void RainbowEffect::Apply(RgbaImage* dst, bool* is_done) {
  Autolock l(lock_);

  // Update rainbow position, if it is still requested.
  if (rainbow_target_x_ >= 0) {
    if (rainbow_effective_x_ >= 0) {
      int distance = rainbow_target_x_ - rainbow_effective_x_;
      int step = distance / fps();
      if (distance && !step) step = (distance > 0 ? 1 : -1);
      rainbow_effective_x_ += step;
    } else {
      rainbow_effective_x_ = rainbow_target_x_;
    }
  }

  // Update rainbow height. Go full height in 1 second.
  int rainbow_height_step = height() / fps();
  if (rainbow_effective_x_ >= 0 && rainbow_target_x_ >= 0) {
    rainbow_height_ = std::min(rainbow_height_ + rainbow_height_step, height());
  } else {
    rainbow_height_ = std::max(rainbow_height_ - rainbow_height_step, 0);
  }

  if (!rainbow_height_) {
    *is_done = true;
    return;
  }

  // Make rainbow pass in 1 second.
  rainbow_shift_ -= (kRainbowSize / fps());
  if (rainbow_shift_ < 0) rainbow_shift_ = kRainbowSize - 1;

  int rainbow_width = rainbow_height_ / 3;
  rainbow_width = std::min(std::max(rainbow_width, 1), width());
  int start_x = std::max(rainbow_effective_x_ - rainbow_width / 2, 0);
  uint32_t* data = reinterpret_cast<uint32_t*>(dst->data());
  for (int y = 0; y < rainbow_height_; ++y) {
    int rainbow_pos =
	static_cast<int>(static_cast<double>(y) / height() * kRainbowSize);
    uint32_t color = rainbow_.at<uint32_t>(
	0, (rainbow_pos + rainbow_shift_) % kRainbowSize);
    uint32_t* start_data = data + (height() - y - 1) * width() + start_x;
    for (int x = 0; x < rainbow_width; ++x) {
      if (rainbow_cycle_ % 2 == 0) {
        start_data[x] = color;
      } else {
        start_data[x] = 0;
      }
    }
  }
  ++rainbow_cycle_;
}
