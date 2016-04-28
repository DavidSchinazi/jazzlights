#ifndef DFSPARKS_EFFECT_COLORFILL_H
#define DFSPARKS_EFFECT_COLORFILL_H
#include <DFSparks_Effect.h>
#include <DFSparks_Palette.h>

namespace dfsparks {

class Colorfill : public Effect {
public:
  Colorfill(int32_t pd, const Palette16 &pl, int st = 1)
      : period_(pd), step_(st), palette_(pl){};

  void render(Matrix &matrix) override;

private:
  int32_t period_;
  int step_;
  const Palette16 &palette_;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_COLORFILL_H */