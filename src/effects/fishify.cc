// Copyright 2016, Igor Chernyshev.

#include "effects/fishify.h"

#include "util/led_layout.h"
#include "util/lock.h"
#include "util/logging.h"

FishifyEffect::FishifyEffect() {}

FishifyEffect::~FishifyEffect() {}

void FishifyEffect::DoInitialize() {}

void FishifyEffect::Destroy() {
  CHECK(false);
}

// TODO(igorc): Use a better mechanism for selecting vivid colors:
// http://stackoverflow.com/questions/470690/how-to-automatically-generate-n-distinct-colors
void FishifyEffect::ApplyOnLeds(LedStrands* strands, bool* is_done) {
  (void) is_done;

  strands->ConvertTo(LedStrands::TYPE_HSL);

  uint32_t min_l = 255;
  uint32_t max_l = 0;
  uint32_t min_s = 255;
  uint32_t max_s = 0;
  uint8_t* all_colors = strands->GetAllColorData();
  for (int i = 0; i < strands->GetTotalLedCount(); ++i) {
    uint8_t* color = all_colors + i * 4;
    if (color[1] < min_l)
      min_l = color[1];
    if (color[1] > max_l)
      max_l = color[1];
    if (color[2] < min_s)
      min_s = color[2];
    if (color[2] > max_s)
      max_s = color[2];
  }

  static const float kMaxLuminance = 255.0 * 0.75;
  float luminance_scale = kMaxLuminance / max_l;
  for (int i = 0; i < strands->GetTotalLedCount(); ++i) {
    uint8_t* color = all_colors + i * 4;
    if (luminance_scale < 1)
      color[1] = static_cast<uint8_t>(luminance_scale * color[1]);

    uint32_t s = static_cast<uint32_t>(color[2]) * 2;
    color[2] = static_cast<uint8_t>(s <= 255 ? s : 255);

    color[0] = color[0] & 0xF8;
  }
}
