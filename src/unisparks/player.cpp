#include "unisparks/player.hpp"

#include <stdlib.h>
#include <assert.h>
#include "unisparks/effects/chess.hpp"
#include "unisparks/effects/flame.hpp"
#include "unisparks/effects/glitter.hpp"
#include "unisparks/effects/overlay.hpp"
#include "unisparks/effects/plasma.hpp"
#include "unisparks/effects/rainbow.hpp"
#include "unisparks/effects/rider.hpp"
#include "unisparks/effects/sequence.hpp"
#include "unisparks/effects/slantbars.hpp"
#include "unisparks/effects/solid.hpp"
#include "unisparks/effects/transform.hpp"
#include "unisparks/effects/threesine.hpp"
#include "unisparks/registry.hpp"
#include "unisparks/renderers/simple.hpp"
#include "unisparks/util/containers.hpp"
#include "unisparks/util/log.hpp"
#include "unisparks/util/math.hpp"
#include "unisparks/util/memory.hpp"
#include "unisparks/util/stream.hpp"
#include "unisparks/util/time.hpp"
#include "unisparks/version.hpp"
// #include "unisparks/effects/scroll.hpp"

namespace unisparks {

using namespace internal;
void  Player::addDefaultEffects2D() {
  static auto synctest = effect([ = ](const Frame& frame) {
      return [ = ](const Pixel& /*pt*/) -> Color {
        Color colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff};
        return colors[int(frame.time / 1000) % 4];
      };
    });
  // static auto synctest = sequence(1000, solid(RED), 1000, solid(GREEN)); //loop(100, sequence(1000, solid(RED), 1000, solid(GREEN)));
  // addDefaultEffect("synctest", synctest, true);

  // addDefaultEffect("synctest", clone(
  //   loop(100, sequence(1000, solid(RED), 1000, solid(GREEN)))
  //   ), true );


  addDefaultEffect("synctest", synctest, false);

  addDefaultEffect("rainbow", clone(
                     rainbow()
                   ), true);

  addDefaultEffect("threesine",
                   clone(threesine()),
                   true
                  );

  addDefaultEffect("rider",
                   clone(rider()),
                   false
                  );

  addDefaultEffect("glitter",
                   clone(glitter()),
                   true
                  );

  addDefaultEffect("slantbars",
                   clone(slantbars()),
                   false
                  );

  addDefaultEffect("plasma",
                   clone(plasma()),
                   true
                  );

  addDefaultEffect("roboscan", clone(
                     unisparks::overlay(alphaLightnessBlend, rider(), transform(ROTATE_LEFT,
                                        rider()))
                   ), false);

  addDefaultEffect("crossbars", clone(
    unisparks::overlay(alphaLightnessBlend, slantbars(), transform(ROTATE_LEFT, slantbars()))
    ), false);

  // addDefaultEffect("flame", clone(
  //                    flame()
  //                  ), false);
}

void render(const Layout& layout, Renderer* renderer,
            const Effect& effect, Frame effectFrame,
            const Effect* overlay, Frame overlayFrame) {
  auto pixels = points(layout);
  auto colors = map(pixels, [&](Point pt) -> Color {
    Pixel px;
    px.coord = pt;
    px.frame = effectFrame;
    int pxidx = 0;
    Color clr = effect.color(px);
    if (overlay) {
      px.frame = overlayFrame;
      px.index = pxidx++;
      clr = alphaBlend(overlay->color(px), clr);
    }
    return clr;
  });

  if (renderer) {
    renderer->render(colors);
  }
}

inline int nextidx(int i, int first, int size) {
  return i >= size - 1 ? first : i + 1;
}

inline int previdx(int i, int first, int size) {
  return i > first ? i - 1 : size - 1;
}

Player::Player() {
  effectContext_ = overlayContext_ = nullptr;
  reset();
}

Player::~Player() {
  end();
}

void Player::end() {
  free(effectContext_);
  free(overlayContext_);
  effectContext_ = overlayContext_ = nullptr;
}

void Player::reset() {
  end();

  ready_ = false;
  strandCount_ = 0;
  effectCount_ = 0;
  effectIdx_ = 0;
  time_ = 0;
  overlayIdx_ = 0;
  overlayTime_ = 0;
  tempo_ = 120;
  metre_ = SIMPLE_QUADRUPLE;

  playlistSize_ = 0;
  track_ = 0;

  paused_ = false;

  network_ = nullptr;

  preferredEffectDuration_ = 10 * ONE_SECOND;

  throttleFps_ = 30;

  lastRenderTime_ = -1;

  fps_ = -1;
  lastFpsProbeTime_ = -1;
  framesSinceFpsProbe_ = -1;
}

Player& Player::clearStrands() {
  strandCount_ = 0;
  return *this;
}

Player& Player::addStrand(const Layout& l, SimpleRenderFunc r) {
  return addStrand(l, make<SimpleRenderer>(r));
}

Player& Player::addStrand(const Layout& l, Renderer& r) {
  end();
  constexpr size_t MAX_STRANDS = sizeof(strands_) / sizeof(*strands_);
  if (strandCount_ >= MAX_STRANDS) {
    fatal("Trying to add too many strands, max=%d", MAX_STRANDS);
  }
  strands_[strandCount_++] = {&l, &r};
  return *this;
}

Player& Player::autoplayEffect(const char* name, bool v) {
  end();
  size_t idx;
  if (!findEffect(name, &idx)) {
    fatal("Can't set autplay for unknown effect '%s'", name);
  }
  effects_[idx].autoplay = v;
  ready_ = false;
  return *this;
}

Player& Player::preferredEffectDuration(Milliseconds v) {
  end();
  preferredEffectDuration_ = v;
  ready_ = false;
  return *this;
}

Player& Player::throttleFps(FramesPerSecond v) {
  end();
  throttleFps_ = v;
  ready_ = false;
  return *this;
}

Player& Player::connect(Network& n) {
  end();
  network_ = &n;
  ready_ = false;
  return *this;
}

Player& Player::addEffect(const char* name, const Effect& effect,
                          bool autoplay) {
  end();
  size_t idx;
  if (!findEffect(name, &idx)) {
    if (effectCount_ >= MAX_EFFECTS) {
      fatal("Trying to add too many effects, max=%d", MAX_EFFECTS);
    }
    idx = effectCount_++;
  }
  effects_[idx] = {&effect, name, autoplay};
  ready_ = false;
  return *this;
}

void Player::addDefaultEffect(const char* name, const Effect& effect,
                              bool autoplay) {
  size_t idx;
  if (!findEffect(name, &idx) && effectCount_ < MAX_EFFECTS) {
    effects_[effectCount_++] = {&effect, name, autoplay};
  }
}

void Player::begin(const Layout& layout, Renderer& renderer,
                   Network& network) {
  reset();
  addStrand(layout, renderer);
  connect(network);
  begin();
}

void Player::begin(const Layout& layout, Renderer& renderer) {
  reset();
  addStrand(layout, renderer);
  begin();
}

void Player::begin(const Layout& layout, SimpleRenderFunc renderer,
                   Network& network) {
  reset();
  addStrand(layout, renderer);
  connect(network);
  begin();
}

void Player::begin(const Layout& layout, SimpleRenderFunc renderer) {
  reset();
  addStrand(layout, renderer);
  begin();
}

void Player::begin() {

  addDefaultEffects2D();

  if (effectCount_ < 1) {
    fatal("Need at least one effect");
  }

  // Fill out playlist
  playlistSize_ = 0;
  for (size_t i = 0; i < effectCount_; ++i) {
    if (effects_[i].autoplay) {
      playlist_[playlistSize_++] = i;
    }
  }

  if (playlistSize_ < 1) {
    fatal("Need at least one autoplay effect");
  }

  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    viewport_ = merge(viewport_, unisparks::bounds(*s->layout));
  }

  size_t ctxsz = 0;
  for (auto efi : Slice<const EffectInfo> {effects_, effectCount_}) {
    size_t sz = efi.effect->contextSize({viewport_, nullptr});
    if (sz > ctxsz) {
      ctxsz = sz;
    }
  }
  if (ctxsz > 0) {
    effectContext_ = malloc(ctxsz);
    overlayContext_ = malloc(ctxsz);
  }

  int pxcnt = 0;
  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    pxcnt += s->layout->pixelCount();
  }
  info("Starting Unisparks player (v%s); strands: %d%s, pixels: %d, effects: %d (%d in playlist), %s",
       UNISPARKS_VERSION,
       strandCount_,
       strandCount_ < 1 ? " (CONTROLLER ONLY!)" : "",
       pxcnt,
       effectCount_,
       playlistSize_,
       network_ ? "networked" : "standalone");

  debug("Registered effects:");
  for (size_t i = 0, pli = 0; i < effectCount_; ++i) {
    if (effects_[i].autoplay) {
      debug("   %2d %s",  ++pli, effects_[i].name);
    } else {
      debug("      %s", effects_[i].name);
    }
  }
  effectIdx_ = overlayIdx_ = track_ = effectCount_;
  switchToPlaylistItem(0);
  ready_ = true;
}

void Player::render() {
  if (!ready_) {
    begin();
  }

  syncToNetwork();
  Milliseconds currTime = timeMillis();
  Milliseconds timeSinceLastRender = currTime - lastRenderTime_;
  if (throttleFps_ > 0
      && timeSinceLastRender > 0
      && timeSinceLastRender < ONE_SECOND / throttleFps_) {
    delay(ONE_SECOND / throttleFps_ - timeSinceLastRender);
    currTime = timeMillis();
    timeSinceLastRender = currTime - lastRenderTime_;
  }

  if (currTime - lastFpsProbeTime_ > ONE_SECOND) {
    fps_ = framesSinceFpsProbe_;
    lastFpsProbeTime_ = currTime;
    framesSinceFpsProbe_ = 0;
  }
  lastRenderTime_ = currTime;
  framesSinceFpsProbe_++;

  render(timeSinceLastRender);
}

void Player::render(Milliseconds dt) {
  if (!ready_) {
    begin();
  }

  if (!paused_) {
    time_ += dt;
    overlayTime_ += dt;
  }
#if 0
  Milliseconds brd = ONE_MINUTE / tempo_;
  //Milliseconds timeSinceDownbeat = (currTime - lastDownbeatTime_) % brd;

  Milliseconds currEffectDuration = brd * int(preferredEffectDuration_ / brd);
#else
  Milliseconds currEffectDuration = preferredEffectDuration_;
#endif

  if (time_ > currEffectDuration) {
    switchToPlaylistItem(nextidx(track_, 0, playlistSize_));
  }

  const Effect* effect = effects_[effectIdx_].effect;
  const Effect* overlay = overlayIdx_ < effectCount_ ?
                          effects_[overlayIdx_].effect : nullptr;

  Frame efr = effectFrame();
  Frame ofr = overlayFrame();

  effect->rewind(efr);
  if (overlay) {
    overlay->rewind(ofr);
  }

  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    unisparks::render(*s->layout, s->renderer, *effect, efr, overlay, ofr);
  }
}

void Player::prev() {
  jump(previdx(track_, 0, playlistSize_));
}

void Player::next() {
  jump(nextidx(track_, 0, playlistSize_));
}

void Player::jump(int index) {
  if (switchToPlaylistItem(index % playlistSize_) && network_) {
    network_->isControllingEffects(true);
  }
}

void Player::play(const char* name) {
  if (syncEffectByName(name, 0) && network_) {
    network_->isControllingEffects(true);
  }
}

bool Player::switchToPlaylistItem(size_t index) {
  if (track_ != index && index < playlistSize_
      && syncEffectByIndex(playlist_[index], 0)) {
    track_ = index;
  }
  return track_ == index;
}

bool Player::syncEffectByIndex(size_t index, Milliseconds time) {
  if (index < effectCount_) {
    if (effectIdx_ != index) {
      if (effectIdx_ < effectCount_) {
        effects_[effectIdx_].effect->end(effectFrame().animation);
      }
      effectIdx_ = index;
      Frame fr = effectFrame();
      effects_[effectIdx_].effect->begin(fr);
      info("Playing effect %s", effectName());
    }
    time_ = time;
  }
  return index == effectIdx_ && time == time_;
}


bool Player::syncEffectByName(const char* name, Milliseconds time) {
  size_t i;
  if (!findEffect(name, &i)) {
    warn("Ignored unknown effect %s", name);
    return false;
  }
  return syncEffectByIndex(i, time);
}

void Player::syncToNetwork() {
  if (!network_) {
    return;
  }

  const char* pattern = effectName();
  Milliseconds time = time_;
  BeatsPerMinute tempo = tempo_;
  if (network_->sync(&pattern, &time, &tempo)) {
    syncEffectByName(pattern, time);
  }
}

void Player::pause() {
  if (!paused_) {
    paused_ = true;
    info("Paused");
  }
}

void Player::resume() {
  if (paused_) {
    paused_ = false;
    info("Resumed");
  }
}

const char* Player::effectName() const {
  return effects_[effectIdx_].name;
}

void Player::overlay(const char* name) {
  size_t idx;
  if (findEffect(name, &idx)) {
    if (overlayIdx_ < effectCount_) {
      effects_[overlayIdx_].effect->end(overlayFrame().animation);
    }
    overlayIdx_ = static_cast<int>(idx);
    overlayTime_ = 0;
    effects_[overlayIdx_].effect->begin(overlayFrame());

    info("Playing overlay %s", overlayName());
  }
}

void Player::clearOverlay() {
  overlayIdx_ = effectCount_;
}

bool Player::findEffect(const char* name, size_t* idx) {
  for (size_t i = 0; i < effectCount_; ++i) {
    if (!strcmp(name, effects_[i].name)) {
      *idx = i;
      return true;
    }
  }
  return false;
}

Frame Player::effectFrame() const {
  Frame frame;
  frame.animation.viewport = viewport_;
  frame.animation.context = effectContext_;
  frame.time = time_;
  frame.tempo = tempo_;
  frame.metre = metre_;
  return frame;
}

Frame Player::overlayFrame() const {
  Frame frame = effectFrame();
  frame.animation.context = overlayContext_;
  frame.time = overlayTime_;
  return frame;
}

const char* Player::command(const char* req) {
  static char res[256];
  const size_t MAX_CMD_LEN = 16;
  bool responded = false;
  char pattern[16];
  Milliseconds time;

  if (!strncmp(req, "status?", MAX_CMD_LEN)) {
    // do nothing
  } else  if (!strncmp(req, "next", MAX_CMD_LEN)) {
    next();
  } else if (!strncmp(req, "prev", MAX_CMD_LEN)) {
    prev();
  } else if (sscanf(req, "play %15s %d", pattern, &time) == 2) {
    play(pattern);
  } else {
    snprintf(res, sizeof(res), "! unknown command");
    responded = true;
  }
  if (!responded) {
    snprintf(res, sizeof(res), "play %s %d", effectName(),
             effectTime());
  }
  debug("[%s] -> [%s]", req, res);
  return res;
}

} // namespace unisparks
