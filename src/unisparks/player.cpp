#include "unisparks/player.hpp"

#include <stdlib.h>
#include <assert.h>
#include "unisparks/effects/chess.hpp"
#include "unisparks/effects/glitter.hpp"
#include "unisparks/effects/overlay.hpp"
#include "unisparks/effects/plasma.hpp"
#include "unisparks/effects/rainbow.hpp"
#include "unisparks/effects/rider.hpp"
#include "unisparks/effects/scroll.hpp"
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
#include "unisparks/util/time.hpp"

namespace unisparks {

using namespace internal;

const Playlist::Item* begin(const Playlist& pl) {
  return pl.items;
}

const Playlist::Item* end(const Playlist& pl) {
  return pl.items + pl.size;
}

static auto syncTestEffect = loop(100,
                                  sequence(1000, solid(RED), 1000, solid(GREEN)));

const Playlist& defaultEffects2D() {
  static auto ef01 = rainbow();
  static auto ef02 = treesine();
  static auto ef03 = rider();
  static auto ef04 = glitter();
  static auto ef05 = slantbars();
  static auto ef06 = plasma();
  static auto ef07 = overlay(alphaLightnessBlend, rider(),
                             transform(ROTATE_LEFT,
                                       rider()));
  static auto ef08 = overlay(alphaLightnessBlend, slantbars(),
                             transform(ROTATE_LEFT,
                                       slantbars()));

  static const Playlist::Item effects[] = {
    {&ef01, "rainbow"},
    {&ef02, "treesine"},
    {&ef03, "rider"},
    {&ef04, "glitter"},
    {&ef05, "slantbars"},
    {&ef06, "plasma"},
    {&ef07, "roboscan"},
    {&ef08, "crossbars"},
  };
  static auto playlist = Playlist{
    &effects[0], sizeof(effects) / sizeof(*effects)
  };
  return playlist;
}

inline int nextidx(int i, int first, int size) {
  return i >= size - 1 ? first : i + 1;
}

inline int previdx(int i, int first, int size) {
  return i > first ? i - 1 : size - 1;
}

inline int find(const Playlist& pl, const char* name) {
  int i = 0;
  for (auto& e : pl) {
    if (!strcmp(e.name, name)) {
      return i;
    }
    i++;
  }
  return -1;
}

Player::Player() {
}

Player::~Player() {
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

  if (!options_.playlist) {
    options_.playlist = &defaultEffects2D();
  }
  if (options_.playlist->size < 1 || !options_.playlist->items) {
    fatal("Need at least one effect");
  }

  int pxcnt = 0;
  for (Strand* s = options_.strands;
       s < options_.strands + options_.strandCount; ++s) {
    pxcnt += s->layout->pixelCount();
  }
  info("Starting player; strands: %d%s, pixels: %d, effects: %d, %s",
       options_.strandCount,
       options_.strandCount < 1 ? " (CONTROLLER ONLY!)" : "",
       pxcnt,
       options_.playlist->size,
       options_.network ? "networked" : "standalone");

  rewind(0, 0);
}

void Player::render() {
  sync();

  Milliseconds currTime = timeMillis();
  Milliseconds timeSinceLastRender = currTime - lastRenderTime_;
  if (options_.throttleFps > 0
      && timeSinceLastRender < ONE_SECOND / options_.throttleFps) {
    delay(ONE_SECOND / options_.throttleFps - timeSinceLastRender);
    currTime = timeMillis();
    timeSinceLastRender = currTime - lastRenderTime_;
  }
  if (lastRenderTime_ < 0) {
    time_ = 0;
  } else {
    if (!paused_) {
      time_ += timeSinceLastRender;
      overlayTime_ += timeSinceLastRender;
    }
    if (currTime - lastFpsProbeTime_ > ONE_SECOND) {
      fps_ = framesSinceFpsProbe_;
      lastFpsProbeTime_ = currTime;
      framesSinceFpsProbe_ = 0;
    }
  }
  lastRenderTime_ = currTime;
  framesSinceFpsProbe_++;

  Milliseconds brd = ONE_MINUTE / tempo_;
  Milliseconds timeSinceDownbeat = (currTime - lastDownbeatTime_) % brd;

  Milliseconds currEffectDuration = brd * int(options_.preferredEffectDuration /
                                    brd);

  if (time_ > currEffectDuration) {
    rewind(nextidx(track_, 0, options_.playlist->size), 0);
  }

  BasicOverlay effect(alphaBlend, overlay_,
                      options_.playlist->items[track_].effect);

  for (Strand* s = options_.strands;
       s < options_.strands + options_.strandCount; ++s) {
    render(effect, time_, tempo_, timeSinceDownbeat, metre_, *s->layout,
           s->renderer);
  }
}


void Player::render(const Effect& effect, Milliseconds time,
                    BeatsPerMinute tempo, Milliseconds timeSinceDownbeat,
                    Metre metre, const Layout& layout, Renderer* renderer) {
  Frame frame;
  frame.time = time;
  frame.tempo = tempo;
  //frame.timeSinceDownbeat = timeSinceDownbeat;
  frame.metre = metre;
  frame.pixelCount = layout.pixelCount();
  frame.origin = layout.bounds().origin;
  frame.size = layout.bounds().size;
  frame.context = nullptr;

  uint8_t context[effect.contextSize(frame)];
  frame.context = context;
  effect.begin(frame);

  auto pixels = layout.pixels();
  auto colors = map(pixels, [&](Point pt) -> Color {
    return effect.color(pt, frame);
  });
 
  if (renderer) {
    renderer->render(colors, layout.pixelCount());
  }
}

void Player::prev() {
  play(previdx(track_, 0, options_.playlist->size));
}

void Player::next() {
  play(nextidx(track_, 0, options_.playlist->size));
}

void Player::play(int index) {
  if (options_.network) {
    options_.network->isControllingEffects(true);
  }
  rewind(index % options_.playlist->size, 0);
}

void Player::rewind(int index, Milliseconds time) {
  if (track_ != index && index >= 0 && index < options_.playlist->size) {
    track_ = index;
    time_ = time;
    info("Playing effect #%d %s", track_ + 1, effectName());
  }
}

void Player::rewind(const char* name, Milliseconds time) {
  int i = find(*options_.playlist, name);
  if (i < 0) {
    warn("Ignored unknown effect %s", name);
  } else {
    rewind(i, time);
  }
}

void Player::sync() {
  if (!options_.network) {
    return;
  }

  const char* pattern = (overlay_ == &syncTestEffect) ? "sync-test" :
                        effectName();
  Milliseconds time = time_;
  BeatsPerMinute tempo = tempo_;
  if (options_.network->sync(&pattern, &time, &tempo)) {
    if (!strcmp(pattern, "sync-test")) {
      syncTest(time);
    } else {
      rewind(pattern, time);
    }
  }
}

void Player::syncTest(Milliseconds ts) {
  info("Playing sync test");
  overlay(syncTestEffect);
  overlayTime_ = ts;
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
  return track_ >= 0 ? options_.playlist->items[track_].name : "none";
}

void Player::overlay(const Effect& ef) {
  overlay_ = &ef;
  overlayTime_ = 0;
}

void Player::clearOverlay() {
  overlay_ = 0;
}


} // namespace unisparks
