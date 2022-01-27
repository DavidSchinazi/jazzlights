#include "unisparks/player.hpp"

#include <cstdio>
#include <stdlib.h>
#include <assert.h>
#include "unisparks/effects/chess.hpp"
#include "unisparks/effects/flame.hpp"
#include "unisparks/effects/glitter.hpp"
#include "unisparks/effects/glow.hpp"
#include "unisparks/effects/overlay.hpp"
#include "unisparks/effects/plasma.hpp"
#include "unisparks/effects/rainbow.hpp"
#include "unisparks/effects/rider.hpp"
#include "unisparks/effects/sequence.hpp"
#include "unisparks/effects/slantbars.hpp"
#include "unisparks/effects/solid.hpp"
#include "unisparks/effects/transform.hpp"
#include "unisparks/effects/threesine.hpp"
#include "unisparks/networks/esp32_ble.hpp"
#include "unisparks/registry.hpp"
#include "unisparks/renderers/simple.hpp"
#include "unisparks/util/containers.hpp"
#include "unisparks/util/log.hpp"
#include "unisparks/util/math.hpp"
#include "unisparks/util/memory.hpp"
#include "unisparks/util/stream.hpp"
#include "unisparks/util/time.hpp"
#include "unisparks/version.hpp"

namespace unisparks {

using namespace internal;

#ifndef ESP32_BLE_RECEIVER
#  define ESP32_BLE_RECEIVER ESP32_BLE
#endif // ESP32_BLE_RECEIVER

#ifndef ESP32_BLE_SENDER
#  define ESP32_BLE_SENDER ESP32_BLE
#endif // ESP32_BLE_SENDER

int comparePrecedence(Precedence leftPrecedence,
                      const DeviceIdentifier& leftDeviceId,
                      Precedence rightPrecedence,
                      const DeviceIdentifier& rightDeviceId) {
  if (leftPrecedence < rightPrecedence) {
    return -1;
  } else if (leftPrecedence > rightPrecedence) {
    return 1;
  }
  return memcmp(leftDeviceId, rightDeviceId, sizeof(DeviceIdentifier));
}

#define KNOWN_PATTERNS \
  X(0x01, "rainbow") \
  X(0x02, "sp-ocean") \
  X(0x03, "sp-heat") \
  X(0x04, "threesine") \
  X(0x05, "sp-lava") \
  X(0x06, "sp-rainbow") \
  X(0x07, "glitter") \
  X(0x08, "sp-party") \
  X(0x09, "sp-cloud") \
  X(0x0a, "plasma") \
  X(0x0b, "sp-forest") \
  X(0x0c, "flame") \
  X(0x0d, "rider") \
  X(0x0e, "slantbars") \
  X(0x0f, "roboscan") \
  X(0x10, "crossbars") \
  X(0x11, "glow") \
  X(0x12, "synctest")

uint32_t encodePattern(const char* patternName) {
#define X(value, name) \
  if (strncmp(name, patternName, 16) == 0) { return value; }
  KNOWN_PATTERNS
#undef X
  return 0x00;
}

void decodePattern(uint32_t encodedPattern, char* patternName) {
#define X(value, name) \
  case value: strncpy(patternName, name, 16); return;
  switch (encodedPattern) {
    KNOWN_PATTERNS
  }
#undef X
  strncpy(patternName, "rainbow", 16);
}

#if ESP32_BLE_SENDER
void writeUint32(uint8_t* data, uint32_t number) {
  data[0] = static_cast<uint8_t>((number & 0xFF000000) >> 24);
  data[1] = static_cast<uint8_t>((number & 0x00FF0000) >> 16);
  data[2] = static_cast<uint8_t>((number & 0x0000FF00) >> 8);
  data[3] = static_cast<uint8_t>((number & 0x000000FF));
}

void writeUint16(uint8_t* data, uint16_t number) {
  data[0] = static_cast<uint8_t>((number & 0xFF00) >> 8);
  data[1] = static_cast<uint8_t>((number & 0x00FF));
}

void SendBle(const DeviceIdentifier& originalSenderId,
             Precedence originalSenderPrecedence,
             const char* effectNameToSend,
             Milliseconds elapsedTimeToSend,
             Milliseconds currentTime) {
  uint8_t blePayload[3 + 6 + 2 + 4 + 4 + 4] = {0xFF, 'L', '1'};
  memcpy(&blePayload[3], originalSenderId, 6);
  writeUint16(&blePayload[3 + 6], originalSenderPrecedence);
  writeUint32(&blePayload[3 + 6 + 2], encodePattern(effectNameToSend));
  writeUint32(&blePayload[3 + 6 + 2 + 4], 0); // Next pattern, will be used later.
  Esp32Ble::SetInnerPayload(sizeof(blePayload),
                            blePayload,
                            3 + 6 + 2 + 4 + 4,
                            currentTime - elapsedTimeToSend,
                            currentTime);
}
#endif // ESP32_BLE_SENDER

#if ESP32_BLE_RECEIVER
uint32_t readUint32(const uint8_t* data) {
  return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
}

uint16_t readUint16(const uint8_t* data) {
  return (data[0] << 8) | (data[1]);
}

bool ReceiveBle(const Esp32Ble::ScanResult& sr,
                Milliseconds currentTime,
                DeviceIdentifier receivedOriginalSenderId,
                Precedence* receivedOriginalSenderPrecedence,
                char* receivedEffectName,
                Milliseconds* receivedElapsedTime) {
  if (sr.innerPayloadLength < 3 + 6 + 2 + 4 + 4 + 4) {
    info("%u Ignoring received BLE with unexpected length %u",
         currentTime, sr.innerPayloadLength);
    return false;
  }
  static constexpr uint8_t prefix[3] = {0xFF, 'L', '1'};
  if (memcmp(sr.innerPayload, prefix, sizeof(prefix)) != 0) {
    info("%u Ignoring received BLE with unexpected prefix", currentTime);
    return false;
  }
  Milliseconds receivedTime =
    Esp32Ble::ReadTimeFromPayload(sr.innerPayloadLength,
                                  sr.innerPayload,
                                  3 + 6 + 2 + 4 + 4);
  if (receivedTime == 0xFFFFFFFF) {
    info("%u Ignoring received BLE with unexpected time", currentTime);
    return false;
  }
  if (sr.receiptTime <= currentTime) {
    const Milliseconds timeSinceReceipt = currentTime - sr.receiptTime;
    if (receivedTime < 0xFFFFFFFF - timeSinceReceipt) {
      receivedTime += timeSinceReceipt;
    } else {
      receivedTime = 0xFFFFFFFF;
    }
  } else {
    error("%u Unexpected receiptTime %u", currentTime, sr.receiptTime);
    return false;
  }
  memcpy(receivedOriginalSenderId, &sr.innerPayload[3], 6);
  *receivedOriginalSenderPrecedence = readUint16(&sr.innerPayload[3 + 6]);
  decodePattern(readUint32(&sr.innerPayload[3 + 6 + 2]), receivedEffectName);
  *receivedElapsedTime = receivedTime;
  return true;
}
#endif // ESP32_BLE_RECEIVER

#if WEARABLE

auto calibration_effect = effect([ = ](const Frame& frame) {
  const bool blink = ((frame.time % 1000) < 500);
  return [ = ](const Pixel& pt) -> Color {
    const int32_t green = 0x00ff00, blue = 0x0000ff, red = 0xff0000;
    const int32_t yellow = green | red, purple = red | blue, white = 0xffffff;
    const int32_t orange = 0xffcc00;
    int32_t yColors[19] = {red, green, blue, yellow,
      purple, orange, white, blue, yellow,
      red, purple, green, orange,
      white, yellow, purple, green, blue, red};

    int32_t col = 0;
    const int32_t x = pt.coord.x;
    const int32_t y = pt.coord.y;
    if (y >= 0 && y < static_cast<int32_t>(sizeof(yColors) / sizeof(yColors[0]))) {
      col = yColors[y];
    }

    if (blink) {
      if (y == 0 &&
          (x == 0 || x == 6 || x == 14)) {
        col = 0;
      } else if (y == 1 &&
                (x == 5 || x == 13 || x == 19)) {
        col = 0;
      }
    }
    return Color(col);
  };
});

#endif // WEARABLE

auto black_effect = solid(BLACK);
auto red_effect = solid(RED);
auto green_effect = solid(GREEN);
auto blue_effect = solid(BLUE);

auto network_effect = [](NetworkStatus network_status) {
  return effect([ = ](const Frame& frame) {
    int32_t color = 0;
    switch (network_status) {
      case INITIALIZING:
        color = 0xff6600;
        break;
      case DISCONNECTED:
        color = 0xff6600;
        break;
      case CONNECTING:
        color = ((frame.time % 500) < 250) ? 0 : 0xff6600;
        break;
      case CONNECTED:
        color = 0x00ff00;
        break;
      case DISCONNECTING:
        color = 0xff6600;
        break;
      case CONNECTION_FAILED:
        color = 0xff0000;
        break;
    }
    return [ = ](const Pixel& pt) -> Color {
      const int32_t x = pt.coord.x;
      const int32_t y = pt.coord.y;
      if ((x % 5) == 1 || (y % 5) == 1) {
        return Color(0xff00ff); // purple
      }
      return Color(color);
    };
  });
};

auto network_effect_initializing = network_effect(INITIALIZING);
auto network_effect_disconnected = network_effect(DISCONNECTED);
auto network_effect_connecting = network_effect(CONNECTING);
auto network_effect_connected = network_effect(CONNECTED);
auto network_effect_disconnecting = network_effect(DISCONNECTING);
auto network_effect_connection_failed = network_effect(CONNECTION_FAILED);

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
#if GLOW_ONLY
  addDefaultEffect("glow", clone(glow()), true);
#else // GLOW_ONLY

  // For testing maximum power draw
  //addDefaultEffect("solid", clone(solid(WHITE)), true);

  addDefaultEffect("synctest", synctest, false);


  addDefaultEffect("rainbow", clone(rainbow()), true);
  addDefaultEffect("sp-ocean", clone(SpinPlasma(OCPOcean)), true);
  addDefaultEffect("sp-heat", clone(SpinPlasma(OCPHeat)), true);
#if !CAMP_SIGN && !ROPELIGHT
  addDefaultEffect("threesine", clone(threesine()), true);
#endif // !CAMP_SIGN && !ROPELIGHT
  addDefaultEffect("sp-lava", clone(SpinPlasma(OCPLava)), true);
  addDefaultEffect("sp-rainbow", clone(SpinPlasma(OCPRainbow)), true);
#if !CAMP_SIGN
  addDefaultEffect("glitter", clone(glitter()), true);
#endif // !CAMP_SIGN
  addDefaultEffect("sp-party", clone(SpinPlasma(OCPParty)), true);
  addDefaultEffect("sp-cloud", clone(SpinPlasma(OCPCloud)), true);
  addDefaultEffect("plasma", clone(plasma()), true);
  addDefaultEffect("sp-forest", clone(SpinPlasma(OCPForest)), true);
#if WEARABLE && !CAMP_SIGN && !ROPELIGHT
  addDefaultEffect("flame", clone(flame()), true);
#endif // WEARABLE && !CAMP_SIGN && !ROPELIGHT

  addDefaultEffect("rider", clone(rider()), false);
  addDefaultEffect("slantbars", clone(slantbars()), false);

  addDefaultEffect("roboscan", clone(
                     unisparks::overlay(alphaLightnessBlend, rider(), transform(ROTATE_LEFT,
                                        rider()))
                   ), false);

  addDefaultEffect("crossbars", clone(
    unisparks::overlay(alphaLightnessBlend, slantbars(), transform(ROTATE_LEFT, slantbars()))
    ), false);
#endif // GLOW_ONLY
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
  powerLimited = false;

  preferredEffectDuration_ = 10 * ONE_SECOND;

  throttleFps_ = 0;

  lastRenderTime_ = -1;
  lastLEDWriteTime_ = -1;
  lastUserInputTime_ = -1;
  followingLeader_ = false;
  memset(leaderDeviceId_, 0, sizeof(leaderDeviceId_));
  leaderPrecedence_ = 0;
  lastLeaderReceiveTime_ = -1;

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

  render(timeSinceLastRender, currTime);
}

void Player::handleSpecial() {
  specialMode_++;
  if (specialMode_ > 6) {
    specialMode_ = 1;
  }
  info("Starting special mode %u", specialMode_);
}

void Player::stopSpecial() {
  if (specialMode_ == 0) {
    return;
  }
  info("Stopping special mode");
  specialMode_ = 0;
}

void Player::render(Milliseconds dt, Milliseconds currentTime) {
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

  if (time_ > currEffectDuration && !loop_) {
    info("%u Exceeded effect duration, switching from %s (index %u)"
         " to next effect %s (index %u)",
         currentTime, effects_[track_].name, track_,
         effects_[nextidx(track_, 0, playlistSize_)].name,
         nextidx(track_, 0, playlistSize_));
    switchToPlaylistItem(nextidx(track_, 0, playlistSize_));
  }

  static constexpr Milliseconds minLEDWriteTime = 10;
  if (lastLEDWriteTime_ >= 0 &&
      currentTime - minLEDWriteTime < lastLEDWriteTime_) {
    return;
  }
  lastLEDWriteTime_ = currentTime;

  const Effect* effect = effects_[effectIdx_].effect;
  const Effect* overlay = overlayIdx_ < effectCount_ ?
                          effects_[overlayIdx_].effect : nullptr;

  Frame efr = effectFrame();
  Frame ofr = overlayFrame();

  switch (specialMode_) {
    case 1:
#if WEARABLE
      effect = &calibration_effect;
#endif // WEARABLE
      break;
    case 2:
      switch (network_->status()) {
        case INITIALIZING:
          effect = &network_effect_initializing;
          break;
        case DISCONNECTED:
          effect = &network_effect_disconnected;
          break;
        case CONNECTING:
          effect = &network_effect_connecting;
          break;
        case CONNECTED:
          effect = &network_effect_connected;
          break;
        case DISCONNECTING:
          effect = &network_effect_disconnecting;
          break;
        case CONNECTION_FAILED:
          effect = &network_effect_connection_failed;
          break;
      }
      break;
    case 3:
      effect = &black_effect;
      break;
    case 4:
      effect = &red_effect;
      break;
    case 5:
      effect = &green_effect;
      break;
    case 6:
      effect = &blue_effect;
      break;
  }

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
  lastUserInputTime_ = timeMillis();
  followingLeader_ = false;
  if (switchToPlaylistItem(index % playlistSize_) && network_) {
    network_->isControllingEffects(true);
  }
#if ESP32_BLE_SENDER
  Esp32Ble::TriggerSendAsap(timeMillis());
#endif // ESP32_BLE_SENDER
}

void Player::play(const char* name) {
  if (syncEffectByName(name, 0) && network_) {
    network_->isControllingEffects(true);
  }
}

bool Player::switchToPlaylistItem(size_t index) {
  loop_ = false;
  if (track_ != index && index < playlistSize_
      && syncEffectByIndex(playlist_[index], 0)) {
    track_ = index;
  }
  return track_ == index;
}

Precedence GetPrecedenceDepreciation(Milliseconds epochTime,
                                     Milliseconds currentTime) {
  if (epochTime >= 0) {
    Milliseconds timeSinceEpoch;
    if (epochTime <= currentTime) {
      timeSinceEpoch = currentTime - epochTime;
    } else {
      error("%u Bad precedence depreciation time %u", currentTime, epochTime);
      timeSinceEpoch = 0;
    }
    if (timeSinceEpoch < 5000) {
      return timeSinceEpoch / 100;
    } else if (timeSinceEpoch < 30000) {
      return 50 + (timeSinceEpoch - 5000) / 1000;
    }
  }
  return 50 + 25;
}

Precedence DepreciatePrecedence(Precedence startPrecedence,
                                Precedence depreciation) {
  if (startPrecedence <= depreciation) {
    return 0;
  }
  return startPrecedence - depreciation;
}

Precedence Player::GetLocalPrecedence(Milliseconds currentTime) {
  Precedence precedence = 1000;
  Precedence depreciation = GetPrecedenceDepreciation(lastUserInputTime_,
                                                      currentTime);
  precedence = DepreciatePrecedence(precedence, depreciation);
  return precedence;
}

Precedence Player::GetLeaderPrecedence(Milliseconds currentTime) {
  Precedence precedence = leaderPrecedence_;
  Precedence depreciation = GetPrecedenceDepreciation(lastUserInputTime_,
                                                      currentTime);
  precedence = DepreciatePrecedence(precedence, depreciation);
  precedence = DepreciatePrecedence(precedence, depreciation);
  return precedence;
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
      lastLEDWriteTime_ = -1;
#if ESP32_BLE_SENDER
      Milliseconds currentTime = timeMillis();
      DeviceIdentifier localDeviceId;
      Precedence followedPrecedence;
      if (followingLeader_) {
        memcpy(localDeviceId, leaderDeviceId_, sizeof(localDeviceId));
        followedPrecedence = GetLeaderPrecedence(currentTime);
        followedPrecedence = DepreciatePrecedence(followedPrecedence, 100);
      } else {
        Esp32Ble::GetLocalAddress(&localDeviceId);
        followedPrecedence = GetLocalPrecedence(currentTime);
      }
      info("%u Sending BLE as %s " DEVICE_ID_FMT " precedence %u",
           currentTime, (followingLeader_ ? "remote" : "local"),
           DEVICE_ID_HEX(localDeviceId), followedPrecedence);
      SendBle(localDeviceId, followedPrecedence,
              effectName(), time, currentTime);
#endif // ESP32_BLE_SENDER
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
  // If effect is in playlist, sync playlist state to it.
  for (size_t t = 0; t < playlistSize_; t++) {
    if (playlist_[t] == i) {
      track_ = t;
      break;
    }
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
    lastLEDWriteTime_ = -1;
    syncEffectByName(pattern, time);
  }
#if ESP32_BLE_RECEIVER
  const Milliseconds currentTime = timeMillis();
  for (const Esp32Ble::ScanResult& sr : Esp32Ble::GetScanResults()) {
    char receivedPattern[17] = {};
    Milliseconds receivedElapsedTime;
    DeviceIdentifier receivedDeviceId;
    Precedence receivedPrecedence;
    if (!ReceiveBle(sr,
                    currentTime,
                    receivedDeviceId,
                    &receivedPrecedence,
                    receivedPattern,
                    &receivedElapsedTime)) {
      continue;
    }
    DeviceIdentifier followedDeviceId;
    Esp32Ble::GetLocalAddress(&followedDeviceId);
    if (memcmp(receivedDeviceId, followedDeviceId, sizeof(followedDeviceId)) == 0) {
      info("%u Ignoring received BLE from ourselves " DEVICE_ID_FMT,
           currentTime, DEVICE_ID_HEX(receivedDeviceId));
      continue;
    }
    Precedence followedPrecedence;
    if (followingLeader_) {
      memcpy(followedDeviceId, leaderDeviceId_, sizeof(followedDeviceId));
      followedPrecedence = GetLeaderPrecedence(currentTime);
    } else {
      followedPrecedence = GetLocalPrecedence(currentTime);
    }
    if (receivedElapsedTime > preferredEffectDuration_ && !loop_) {
      // Ignore this message because the sender already switched effects.
      info("%u Ignoring received BLE with time past duration", currentTime);
      continue;
    }
    if (comparePrecedence(followedPrecedence, followedDeviceId,
                          receivedPrecedence, receivedDeviceId) >= 0) {
      info("%u Ignoring received BLE from " DEVICE_ID_FMT
           " precedence %u because our %s " DEVICE_ID_FMT " precedence %u is higher",
           currentTime, DEVICE_ID_HEX(receivedDeviceId), receivedPrecedence,
           (followingLeader_ ? "remote" : "local"),
           DEVICE_ID_HEX(followedDeviceId), followedPrecedence);
      continue;
    }
    if (!followingLeader_) {
      info("%u Switching from local " DEVICE_ID_FMT " to leader"
           " " DEVICE_ID_FMT " precedence %u prior precedence %u",
           currentTime, DEVICE_ID_HEX(followedDeviceId),
           DEVICE_ID_HEX(receivedDeviceId),
           receivedPrecedence, followedPrecedence);
    } else if (memcmp(leaderDeviceId_, receivedDeviceId, sizeof(leaderDeviceId_)) != 0) {
      info("%u Picking new leader " DEVICE_ID_FMT " precedence %u"
           " over " DEVICE_ID_FMT " precedence %u",
           currentTime, DEVICE_ID_HEX(receivedDeviceId), receivedPrecedence,
           DEVICE_ID_HEX(leaderDeviceId_), followedPrecedence);
    } else {
      info("%u Keeping leader " DEVICE_ID_FMT " precedence %u"
           " prior precedence %u",
           currentTime, DEVICE_ID_HEX(receivedDeviceId), receivedPrecedence,
           followedPrecedence);
    }
    followingLeader_ = true;
    memcpy(leaderDeviceId_, receivedDeviceId, sizeof(leaderDeviceId_));
    leaderPrecedence_ = receivedPrecedence;
    lastLeaderReceiveTime_ = currentTime;
    info("%u Received BLE: switching to pattern %s elapsedTime %u",
         currentTime, receivedPattern, receivedElapsedTime);
    lastLEDWriteTime_ = -1;
    syncEffectByName(receivedPattern, receivedElapsedTime);
  }
#endif // ESP32_BLE_RECEIVER
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

void Player::loopOne() {
  if (loop_) {
    return;
  }
  info("Looping");
  loop_ = true;
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
