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
// #include "unisparks/effects/scroll.hpp"
#include "unisparks/effects/sequence.hpp"
#include "unisparks/effects/slantbars.hpp"
#include "unisparks/effects/solid.hpp"
#include "unisparks/effects/transform.hpp"
#include "unisparks/effects/treesine.hpp"
#include "unisparks/renderers/simple.hpp"
#include "unisparks/util/log.hpp"
#include "unisparks/util/math.hpp"
#include "unisparks/util/new.hpp"
#include "unisparks/util/stream.hpp"
#include "unisparks/util/containers.hpp"
#include "unisparks/util/time.hpp"

namespace unisparks {

using namespace internal;

Slice<EffectInfo> defaultEffects2D() {
  static auto eff01 = rainbow();
  static auto eff02 = treesine();
  static auto eff03 = rider();
  static auto eff04 = glitter();
  static auto eff05 = slantbars();
  static auto eff06 = plasma();
  static auto eff07 = overlay(alphaLightnessBlend, rider(),
                              transform(ROTATE_LEFT,
                                        rider()));
  static auto eff08 = overlay(alphaLightnessBlend, slantbars(),
                             transform(ROTATE_RIGHT, slantbars()));
  static auto eff09 = flame();
  static auto synctest = loop(100, sequence(1000, solid(RED), 1000,
                              solid(GREEN)));

  static EffectInfo effects[] = {
    {&synctest, "synctest", false},
    {&eff01, "rainbow", true},
    {&eff02, "treesine", true},
    {&eff03, "rider", true},
    {&eff04, "glitter", true},
    {&eff05, "slantbars", true},
    {&eff06, "plasma", true},
    {&eff07, "roboscan", true},
    {&eff08, "crossbars", true},
    {&eff09, "flame", true},
  };
  return Slice<EffectInfo> {effects, sizeof(effects) / sizeof(*effects)};
}

void render(const Layout& layout, Renderer* renderer, 
            const Effect& effect, Frame effectFrame,
            const Effect* overlay, Frame overlayFrame) {
  auto pixels = layout.pixels();
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
}

Player::~Player() {
  free(effectContext_);
  free(overlayContext_);
}

void Player::begin(const Layout& layout, Renderer& renderer,
                   Network& network) {
  firstStrand_ = {&layout, &renderer};
  PlayerOptions opt;
  opt.strands = &firstStrand_;
  opt.strandCount = 1;
  opt.network = &network;
  begin(opt);
}

void Player::begin(const Layout& layout, Renderer& renderer) {
  firstStrand_ = {&layout, &renderer};
  PlayerOptions opt;
  opt.strands = &firstStrand_;
  opt.strandCount = 1;
  begin(opt);
}

void Player::begin(const Layout& layout, SimpleRenderFunc renderer,
                   Network& network) {
  basicRenderer_ = SimpleRenderer(renderer);
  firstStrand_ = {&layout, &basicRenderer_};
  PlayerOptions opt;
  opt.strands = &firstStrand_;
  opt.strandCount = 1;
  opt.network = &network;
  begin(opt);
}

void Player::begin(const Layout& layout, SimpleRenderFunc renderer) {
  basicRenderer_ = SimpleRenderer(renderer);
  firstStrand_ = {&layout, &basicRenderer_};
  PlayerOptions opt;
  opt.strands = &firstStrand_;
  opt.strandCount = 1;
  begin(opt);
}

void Player::begin(PlayerOptions options) {
  options_ = options;

  if (options_.strands && options_.strandCount > 0) {
    firstStrand_ = options_.strands[0];
  }

  for (auto& ef : defaultEffects2D()) {
    enrollEffect(ef);
  }
  if (options_.customEffects && options_.customEffectCount > 0) {
    auto efs = Slice<const EffectInfo> {
      options_.customEffects, options_.customEffectCount
    };
    for (auto& ef : efs) {
      enrollEffect(ef);
    }
  }
  if (effectCount_ < 1) {
    fatal("Need at least one effect");
  }

  if (playlistSize_ < 1) {
    fatal("Need at least one autoplay effect");
  }

  for (Strand* s = options_.strands;
       s < options_.strands + options_.strandCount; ++s) {
    viewport_ = merge(viewport_, s->layout->bounds());
  }

  size_t ctxsz = 0;
  for (auto it : Slice<const EffectInfo> {effects_, effectCount_}) {
    size_t sz = it.effect->contextSize({viewport_, nullptr});
    if (sz > ctxsz) {
      ctxsz = sz;
    }
  }
  if (ctxsz > 0) {
    effectContext_ = malloc(ctxsz);
    overlayContext_ = malloc(ctxsz);
  }

  int pxcnt = 0;
  for (Strand* s = options_.strands;
       s < options_.strands + options_.strandCount; ++s) {
    pxcnt += s->layout->pixelCount();
  }
  info("Starting player; strands: %d%s, pixels: %d, effects: %d (%d in playlist), %s",
       options_.strandCount,
       options_.strandCount < 1 ? " (CONTROLLER ONLY!)" : "",
       pxcnt,
       effectCount_,
       playlistSize_,
       options_.network ? "networked" : "standalone");
  debug("Registered effects:");
  for(size_t i = 0, pli=0; i<effectCount_; ++i) {
    if (effects_[i].autoplay) {
      debug("   %2d %s",  ++pli, effects_[i].name);
    }
    else {
      debug("      %s", effects_[i].name);
    }
  }
  effectIdx_ = overlayIdx_ = track_ = effectCount_;
  switchToPlaylistItem(0);
}

void Player::render() {
  syncToNetwork();
  Milliseconds currTime = timeMillis();
  Milliseconds timeSinceLastRender = currTime - lastRenderTime_;
  if (options_.throttleFps > 0
      && timeSinceLastRender > 0 && timeSinceLastRender < ONE_SECOND / options_.throttleFps) {
    delay(ONE_SECOND / options_.throttleFps - timeSinceLastRender);
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
  if (!paused_) {
    time_ += dt;
    overlayTime_ += dt;
  }

  Milliseconds brd = ONE_MINUTE / tempo_;
  //Milliseconds timeSinceDownbeat = (currTime - lastDownbeatTime_) % brd;

  Milliseconds currEffectDuration = brd * int(options_.preferredEffectDuration /
                                    brd);

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

  for (Strand* s = options_.strands;
       s < options_.strands + options_.strandCount; ++s) {
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
  if (switchToPlaylistItem(index % playlistSize_) && options_.network) {
    options_.network->isControllingEffects(true);
  }
}

void Player::play(const char* name) {
  if (syncEffectByName(name, 0) && options_.network) {
    options_.network->isControllingEffects(true);
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
      effects_[effectIdx_].effect->begin(effectFrame());
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
  if (!options_.network) {
    return;
  }

  const char* pattern = effectName();
  Milliseconds time = time_;
  BeatsPerMinute tempo = tempo_;
  if (options_.network->sync(&pattern, &time, &tempo)) {
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

void Player::enrollEffect(const EffectInfo& ei) {
  size_t idx;
  if (findEffect(ei.name, &idx)) {
    effects_[idx] = ei;
  } else if (effectCount_ < sizeof(effects_) / sizeof(*effects_)) {
    idx = effectCount_++;
    effects_[idx] = ei;
    if (ei.autoplay) {
      playlist_[playlistSize_++] = idx;
    }
  }
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

} // namespace unisparks
