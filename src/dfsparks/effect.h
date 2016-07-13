#ifndef DFSPARKS_EFFECT_H
#define DFSPARKS_EFFECT_H
#include "dfsparks/pixels.h"
#include "dfsparks/timer.h"
#include <assert.h>
#include <stdint.h>

namespace dfsparks {
class Player;


/**
 * Effect base class.
 */
class Effect {
public:
  Effect() noexcept = default;
  virtual ~Effect() noexcept = default;

  void render(Player& player);

  int32_t preferredDuration() const {return 10000; }
  
protected:
  int32_t timeElapsed() const;
  int32_t timeSinceBeat() const;
  int32_t cycleDuration() const;
  uint8_t cycleHue() const;
  
private:
  virtual void doRender(Pixels &) = 0;

  Player *player_ = nullptr;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_H */
