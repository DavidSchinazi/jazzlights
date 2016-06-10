// Copyright 2016, Igor Chernyshev.

#ifndef EFFECTS_PASSTHROUGH_H_
#define EFFECTS_PASSTHROUGH_H_

#include "model/effect.h"
#include "util/pixels.h"

class PassthroughEffect : public Effect {
 public:
  PassthroughEffect();
  ~PassthroughEffect() override;

  void SetImage(const RgbaImage& image);

  void Apply(RgbaImage* dst, bool* is_done) override;

  void Destroy() override;

 protected:
  void DoInitialize() override;

 private:
  PassthroughEffect(const PassthroughEffect& src);
  PassthroughEffect& operator=(const PassthroughEffect& rhs);

  RgbaImage image_;
};

#endif  // EFFECTS_PASSTHROUGH_H_
