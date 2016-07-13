#include "dfsparks/effect.h"
#include "dfsparks/player.h"

#include <assert.h>

namespace dfsparks {

void Effect::render(Player& player) {
  player_ = &player;
  doRender(player_->pixels());
  player_ = nullptr;
}

int32_t Effect::timeElapsed() const {
  return player_ ? player_->timeElapsed() : -1;
}

int32_t Effect::timeSinceBeat() const {
  return player_ ? player_->timeSinceBeat() : -1;
}

int32_t Effect::cycleDuration() const {
  return player_ ? player_->cycleDuration() : 0;
}

uint8_t Effect::cycleHue() const {
  return player_ ? player_->cycleHue() : 0;
}

} // namespace dfsparks
