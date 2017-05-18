#include "unisparks/player.h"
#include "unisparks/log.h"
#include "unisparks/math.h"
#include "unisparks/timer.h"
#include <stdlib.h>

namespace unisparks {

/** 
 * Add two conceptually unsigned integers handling
 * wrap/overflow.
 */
inline int32_t addwrap(int32_t a, int32_t b) {
  int32_t s = a + b;
  return s >= 0 ? s : s - INT32_MIN;
}

// --------------------------------------------------------------------
// Player
// --------------------------------------------------------------------
Player::Player(Pixels& pixels, const Playlist& playlist) : playlist_(playlist), pixels_(pixels) {
  renderBufSize_ = 0;
  for(int i=0; i<playlist_.size(); ++i) {
    if (renderBufSize_ < playlist_[i]->renderBufSize(pixels_)) {
      renderBufSize_ = playlist_[i]->renderBufSize(pixels_);
    }
  }
  renderBuf_ = new uint8_t[renderBufSize_];
  play(playlist_[0], LOW_PRIORITY);
}

Player::~Player() { 
  delete[] renderBuf_;
}

void Player::play(int i) {
  track_ = i % playlist_.size();
  if (track_ < 0) {
    track_ += playlist_.size();
  }
  play(playlist_[track_], HIGH_PRIORITY);
}

void Player::next() { 
  track_ = track_ < playlist_.size() - 1 ? track_ + 1 : 0; 
  play(playlist_[track_], HIGH_PRIORITY);
}

void Player::prev() { 
  track_ = track_ > 0 ? track_ - 1 : playlist_.size() - 1;
  play(playlist_[track_], HIGH_PRIORITY);
}

void Player::render() {
  if (!renderer_) {
    return;
  }
  int32_t currTs = timeMillis();
  int32_t dt = addwrap(currTs, -frameTs_);
  frameTs_ = currTs;
  frame_.timeElapsed = addwrap(frame_.timeElapsed, dt);
  frame_.timeSinceBeat = addwrap(frame_.timeSinceBeat, dt);
  if (mode_ != play_forever) {
    frame_.timeRemaining -= dt;
  }
  if (frame_.timeRemaining <= 0) {
    advance();
  }
  renderer_->render(frame_);
}

void Player::play(const Effect *ef, int priority) {
  assert(ef);
  if (priority_ > priority) {
    return;
  }
  if (renderer_) {
    renderer_->~Renderer();
  }
  if (!renderer_ || &renderer_->effect() != ef) {
    renderer_ = ef->start(pixels_, renderBuf_, renderBufSize_);
    priority_ = priority;
    frame_.timeElapsed = 0;
    frame_.timeRemaining = ef->preferredDuration();
    info("player %p selected effect %s", this, effectName());
  }
}

void Player::advance() {
    switch(mode_) {
      case cycle_one:
        play(playlist_[track_], LOW_PRIORITY);
        break;
      case cycle_all:
        track_ = track_ < playlist_.size() - 1 ? track_ + 1 : 0; 
        play(playlist_[track_], LOW_PRIORITY);
        break;
      case shuffle_all:
        track_ = ::rand() % playlist_.size();
        play(playlist_[track_], LOW_PRIORITY);
        break;
      case play_forever:
        /* do nothing */;
    }
}

void Player::sync(const Effect& ef, int priority, Frame fr) {
  play(&ef, priority);
  Frame &ofr = frame_;
  ofr.timeElapsed = fr.timeElapsed;
  ofr.timeRemaining = fr.timeRemaining;
  ofr.timeSinceBeat = ofr.timeSinceBeat < fr.timeSinceBeat ? 
      ofr.timeSinceBeat : fr.timeSinceBeat;
}

const char *Player::effectName() const {
  return renderer_ ? renderer_->effect().name() : "_none";
}

    
// --------------------------------------------------------------------
// NetworkPlayer
// --------------------------------------------------------------------

NetworkPlayer::NetworkPlayer(Pixels& px, Network &n, const Playlist& pl) : Player(px, pl), netwrk(n),
  networkStatusEffect_(netwrk) {
  netwrk.add(*this);
}

NetworkPlayer::~NetworkPlayer() {
  netwrk.remove(*this);
}

void NetworkPlayer::showStatus() {
  play(&networkStatusEffect_, LOW_PRIORITY);
}

void NetworkPlayer::showStatus(bool v) {
  if (v) {
    showStatus();
  } else {
    // do nothing, player will automatically move on to the 
    // next effect in a few seconds or when user presses anything
  }
}

bool NetworkPlayer::isShowingStatus() const {
  return !strcmp(effectName(), NetworkStatus::name);
}

void NetworkPlayer::render() {
  auto curr_time = timeMillis();
  const char *name = effectName();
  if (mode_ == MASTER && name[0] != '_' && (tx_track_ != track() || curr_time - tx_time_ > tx_interval_)) {
    NetworkFrame fr;
    fr.pattern = name;
    fr.timeElapsed = frame().timeElapsed;
    fr.timeSinceBeat = frame().timeSinceBeat;
    assert(fr.timeElapsed >= 0);
    assert(fr.timeSinceBeat >= 0);
    netwrk.broadcast(fr);
    tx_track_ = track();
    tx_time_ = curr_time;
  }

  Player::render();
}

void NetworkPlayer::onReceived(Network&, const NetworkFrame& fr) {
  int32_t currTime = timeMillis();
  if (mode_ == SLAVE) {
    const Effect *ef = playlist_.find(fr.pattern);
    if (!ef) {
        info("Can't play unknown effect '%s'", fr.pattern);
        return;
    }
    if (fr.timeElapsed < 0) {
      info("Ignored packet with negative elapsed time %d", fr.timeElapsed);
      return;
    }
    if (fr.timeSinceBeat < 0) {
        info("Ignored packet with negative time since beat %d", fr.timeSinceBeat);
        return;
    }
    sync(*ef, LOW_PRIORITY, {fr.timeElapsed, rx_timeout_, fr.timeSinceBeat});
  }
  rx_time_ = currTime;
}


} // namespace unisparks
