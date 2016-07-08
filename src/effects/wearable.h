// Copyright 2016, Igor Chernyshev.

#ifndef EFFECTS_WEARABLE_H_
#define EFFECTS_WEARABLE_H_

#include "model/effect.h"
#include "util/pixels.h"
#include "dfsparks/player.h"
#include "dfsparks/networks/udpsocket.h"

class WearableEffect : public Effect {
 public:
  WearableEffect();
  ~WearableEffect() override;

  // Sets werable's effect number. Negative numbers are used with specific
  // presets from ProjectM.
  void SetEffect(int id);

  void ApplyOnImage(RgbaImage* dst, bool* is_done) final;

  //void ApplyOnLeds(LedStrands* strands, bool* is_done) final;
 
  void Destroy() final;

 protected:
  void DoInitialize() final;

 private:
  WearableEffect(const WearableEffect& src);
  WearableEffect& operator=(const WearableEffect& rhs);

  dfsparks::Pixels *pixels = nullptr;
  dfsparks::NetworkPlayer *player = nullptr;
  int effect = 0;
};

#endif  // EFFECTS_WEARABLE_H_
