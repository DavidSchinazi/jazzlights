#include "dfsparks/player.h"
#include "dfsparks/log.h"
#include "dfsparks/math.h"
#include <stdlib.h>

namespace dfsparks {

// --------------------------------------------------------------------
// Player
// --------------------------------------------------------------------
Player::Player(Pixels& pixels) : repertoire_(pixels), playlist_(repertoire_), effect_(&playlist_.currentEffect()) {
  pixels_ = &pixels;
}
Player::~Player() { 
}

void Player::render() {
  assert(effect_);
  int32_t curr_ts = timeMillis();
  remaining_time_ -= curr_ts - frame_ts_;
  elapsed_time_ += curr_ts - frame_ts_;
  time_since_beat_ += curr_ts - frame_ts_;
  frame_ts_ = curr_ts;

  if (remaining_time_ <= 0) {
    priority_ = LOW_PRIORITY; // so that server can take over
    switch(mode_) {
      case cycle_all:
        doPlay(playlist_.next(), LOW_PRIORITY);
        break;
      case shuffle_all:
        doPlay(playlist_.random(), LOW_PRIORITY);
        break;
      case play_forever:
        remaining_time_ = effect_->preferredDuration(); 
        break;
    }
  }

  if (showStatus_) {
    doRenderStatus();
  }
  else {
    effect_->render(*this);
  }
}

void Player::doPlay(Effect& ef, int priority) {
  doPlay(ef, priority, 0, ef.preferredDuration(), 0);
}

void Player::doPlay(Effect &ef, int priority, int32_t elapsed, int32_t remaining, uint8_t cycle_hue) {
  if (priority_ > priority) {
    return;
  }

  effect_ = &ef;
  elapsed_time_ = elapsed;
  remaining_time_ = remaining;
  cycle_hue_ = cycle_hue;
  priority_ = priority;

  if (priority_ > LOW_PRIORITY) {
    showStatus_ = false;
  }
  info("player %p selected effect %s", this, effectName());
}

void Player::doRenderStatus() {
  pixels().fill(0x00ff00);
}

const char *Player::effectName() const {
  const char *name = "unnamed";
  repertoire_.find(*effect_, &name, nullptr);
  return name;
}

Effect* Player::findEffect(const char *name) const {
  Effect* ef = nullptr;
  repertoire_.find(name, &ef, nullptr);
  return ef;
}

// --------------------------------------------------------------------
// NetworkPlayer
// --------------------------------------------------------------------

NetworkPlayer::NetworkPlayer(Pixels& pixels, Network &n) : Player(pixels), netwrk(n) {
  netwrk.add(*this);
}

NetworkPlayer::~NetworkPlayer() {
  netwrk.remove(*this);
}

void NetworkPlayer::render() {
  auto curr_time = timeMillis();
  const char *name = effectName();
  if (mode_ == MASTER && name[0] != '_' && (tx_track_ != track() || curr_time - tx_time_ > tx_interval_)) {
    Message::Frame fr;
    strncpy(fr.pattern, name, sizeof(fr.pattern));
    fr.elapsed_ms = timeElapsed();
    fr.beat_ms = timeSinceBeat();
    netwrk.broadcast(fr);
    tx_track_ = track();
    tx_time_ = curr_time;
  }
  Player::render();
}

void NetworkPlayer::doRenderStatus() {
  int32_t t = timeMillis();
  uint32_t color = 0x000000;

  switch(netwrk.status()) {
    case Network::disconnected: 
    case Network::disconnecting: 
      color = 0xff6600; 
      break;
    case Network::connecting: 
      color = t % 500 < 250 ? 0 : 0xff6600;
      break;
    case Network::connected: 
      color = t - rx_time_ < 100 ? 0 : 0x00ff00; 
      break;
    case Network::connection_failed: 
      color = 0xff0000; 
      break;
  }
  pixels().fill(color);
}

void NetworkPlayer::onReceived(Network&, const Message::Frame& fr) {
  int32_t currTime = timeMillis();
  if (mode_ == SLAVE) {
    Effect *ef = findEffect(fr.pattern);
    if (ef) {
      doPlay(*ef, LOW_PRIORITY, fr.elapsed_ms, rx_timeout_, 0);
    }
    else {
      const char *altname = 0;
      if (fr.hue_dev < 50) {
        altname = "glitter";
        doPlay(*findEffect(altname), LOW_PRIORITY, fr.elapsed_ms, rx_timeout_, fr.hue_med ? fr.hue_med : 255);
      } else {
        altname = "radiaterainbow";        
        doPlay(*findEffect(altname), LOW_PRIORITY, fr.elapsed_ms, rx_timeout_, 0);
      }
      info("Can't play unknown effect '%s', using '%s' instead", fr.pattern, altname);
    }
  }
  rx_time_ = currTime;
}


} // namespace dfsparks
