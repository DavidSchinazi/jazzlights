// Copyright 2016, Igor Chernyshev.

#ifndef EFFECTS_WEARABLE_H_
#define EFFECTS_WEARABLE_H_

#include "model/effect.h"
#include "util/pixels.h"

class WearableEffect : public Effect {
 public:
  WearableEffect();
  ~WearableEffect() override;

  // Sets werable's effect number. Negative numbers are used with specific
  // presets from ProjectM.
  void SetEffect(int id);

  void ApplyOnImage(RgbaImage* dst, bool* is_done) override;

  void Destroy() override;

 protected:
  void DoInitialize() override;

 private:
  WearableEffect(const WearableEffect& src);
  WearableEffect& operator=(const WearableEffect& rhs);
};

#endif  // EFFECTS_WEARABLE_H_
