// Copyright 2016, Igor Chernyshev.

#ifndef EFFECTS_FISHIFY_H_
#define EFFECTS_FISHIFY_H_

#include "model/effect.h"
#include "util/pixels.h"

// This effect makes colors more vivid, for better effectiveness on DiscoFish.
class FishifyEffect : public Effect {
 public:
  FishifyEffect();
  ~FishifyEffect() override;

  void ApplyOnLeds(LedStrands* strands, bool* is_done) override;

  void Destroy() override;

 protected:
  void DoInitialize() override;

 private:
  FishifyEffect(const FishifyEffect& src);
  FishifyEffect& operator=(const FishifyEffect& rhs);
};

#endif  // EFFECTS_FISHIFY_H_
