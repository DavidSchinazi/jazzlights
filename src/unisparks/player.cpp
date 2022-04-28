#include "unisparks/player.hpp"

#include <cstdio>
#include <stdlib.h>
#include <assert.h>
#include "unisparks/effects/chess.hpp"
#include "unisparks/effects/flame.hpp"
#include "unisparks/effects/glitter.hpp"
#include "unisparks/effects/glow.hpp"
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
                      const NetworkDeviceId& leftDeviceId,
                      Precedence rightPrecedence,
                      const NetworkDeviceId& rightDeviceId) {
  if (leftPrecedence < rightPrecedence) {
    return -1;
  } else if (leftPrecedence > rightPrecedence) {
    return 1;
  }
  return leftDeviceId.compare(rightDeviceId);
}

#if WEARABLE

auto calibration_effect = effect("calibration", [](const Frame& frame) {
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

auto black_effect = solid(BLACK, "black");
auto red_effect = solid(RED, "red");
auto green_effect = solid(GREEN, "green");
auto blue_effect = solid(BLUE, "blue");

auto network_effect = [](NetworkStatus network_status, const std::string& name) {
  return effect(std::string("network-") + name, [ = ](const Frame& frame) {
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

auto network_effect_initializing = network_effect(INITIALIZING, "init");
auto network_effect_disconnected = network_effect(DISCONNECTED, "disconnected");
auto network_effect_connecting = network_effect(CONNECTING, "connecting");
auto network_effect_connected = network_effect(CONNECTED, "connected");
auto network_effect_disconnecting = network_effect(DISCONNECTING, "disconnecting");
auto network_effect_connection_failed = network_effect(CONNECTION_FAILED, "failed");
auto synctest = effect("synctest", [](const Frame& frame) {
    return [ = ](const Pixel& /*pt*/) -> Color {
      Color colors[] = {0xff0000, 0x00ff00, 0x0000ff, 0xffffff};
      return colors[int(frame.time / 1000) % 4];
    };
  });

constexpr bool patternIsReserved(PatternBits pattern) {
  // Patterns with lowest byte zero are reserved.
  return (pattern & 0xFF) == 0;
}

// bitNum is [1-32] starting from the highest bit.
constexpr bool patternbit(PatternBits pattern, uint8_t bitNum) {
  return (pattern & (1 << (sizeof(PatternBits) * 8 - bitNum))) != 0;
}

PatternBits computeNextPattern(PatternBits pattern) {
  static_assert(sizeof(PatternBits) == 4, "32bits");
  // This code is inspired by xorshift, amended to only require 32 bits of
  // state. This algorithm was informed by 10 minutes of Googling and a half
  // bottle of Malbec. It is guaranteed to produce numbers.
	pattern ^= pattern << 13;
	pattern ^= pattern >> 17;
	pattern ^= pattern << 5;
  pattern += 0x1337;
  // Apparently xorshift doesn't have great entropy in the lower bits, so let's
  // move those around just because we can.
  const uint8_t shift_offset = (pattern / 16384) % 32;
	pattern = (pattern << shift_offset) | (pattern >> (32 - shift_offset));
  if (pattern == 0) { pattern = 0xDF123456; }
  while (patternIsReserved(pattern)) {
    // Skip reserved patterns.
    pattern = computeNextPattern(pattern);
  }
  return pattern;
}

auto spin_rainbow_pattern = clone(rainbow());
auto spin_forest_pattern = clone(SpinPlasma(OCPforest));
auto spin_party_pattern = clone(SpinPlasma(OCPparty));
auto spin_cloud_pattern = clone(SpinPlasma(OCPcloud));
auto spin_ocean_pattern = clone(SpinPlasma(OCPocean));
auto spin_lava_pattern = clone(SpinPlasma(OCPlava));
auto spin_heat_pattern = clone(SpinPlasma(OCPheat));
auto flame_pattern = clone(flame());
auto glitter_pattern = clone(glitter());
auto threesine_pattern = clone(threesine());
auto rainbow_pattern = clone(rainbow());

Effect* patternFromBits(PatternBits pattern) {
  if (patternIsReserved(pattern)) {
    return &red_effect;
  } else {
    if (patternbit(pattern, 1)) { // spin
      if (patternbit(pattern, 2)) { // nature
        if (patternbit(pattern, 3)) { // rainbow
          return &rainbow_pattern;
        } else { // frolick
          if (patternbit(pattern, 4)) { // forest
            return &spin_forest_pattern;
          } else { // party
            return &spin_party_pattern;
          }
        }
      } else { // hot&cold
        if (patternbit(pattern, 3)) { // cold
          if (patternbit(pattern, 4)) { // cloud
            return &spin_cloud_pattern;
          } else { // ocean
            return &spin_ocean_pattern;
          }
        } else { // hot
          if (patternbit(pattern, 4)) { // lava
            return &spin_lava_pattern;
          } else { // heat
            return &spin_heat_pattern;
          }
        }
      }
    } else { // custom
      if (patternbit(pattern, 2)) { // sparkly
        if (patternbit(pattern, 3)) { // flame
          return &flame_pattern;
        } else { // glitter
          return &glitter_pattern;
        }
      } else { // shiny
        if (patternbit(pattern, 3)) { // threesine
          return &threesine_pattern;
        } else { // rainbow
          return &rainbow_pattern;
        }
      }
    }
  }
  fatal("Failed to pick an effect %s",
        displayBitsAsBinary(pattern).c_str());
}

Effect* Player::currentEffect() const {
  return patternFromBits(currentPattern_);
}

void render(const Layout& layout, Renderer* renderer,
            const Effect& effect, Frame effectFrame) {
  auto pixels = points(layout);
  auto colors = map(pixels, [&](Point pt) -> Color {
    Pixel px;
    px.coord = pt;
    px.frame = effectFrame;
    Color clr = effect.color(px);
    return clr;
  });

  if (renderer) {
    renderer->render(colors);
  }
}

Player::Player() {
  reset();
}

Player::~Player() {
  end();
}

void Player::end() {
  free(effectContext_);
  effectContext_ = nullptr;
  effectContextSize_ = 0;
}

void Player::reset() {
  end();

  ready_ = false;
  loop_ = false;
  strandCount_ = 0;
  elapsedTime_ = 0;
  currentPattern_ = 0x12345678;
  tempo_ = 120;
  metre_ = SIMPLE_QUADRUPLE;

  paused_ = false;

  network_ = nullptr;
  powerLimited = false;

  throttleFps_ = 0;

  lastRenderTime_ = -1;
  lastLEDWriteTime_ = -1;
  lastUserInputTime_ = -1;
  followingLeader_ = false;
  leaderDeviceId_ = NetworkDeviceId();
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
  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    viewport_ = merge(viewport_, unisparks::bounds(*s->layout));
  }

  int pxcnt = 0;
  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    pxcnt += s->layout->pixelCount();
  }
  info("Starting Unisparks player %s (v%s); strands: %d%s, pixels: %d, %s",
       BOOT_MESSAGE,
       UNISPARKS_VERSION,
       strandCount_,
       strandCount_ < 1 ? " (CONTROLLER ONLY!)" : "",
       pxcnt,
       network_ ? "networked" : "standalone");

  ready_ = true;
  nextInner(timeMillis());
}

void Player::render(Milliseconds currentTime) {
  if (!ready_) {
    begin();
  }

  syncToNetwork(currentTime);
  Milliseconds timeSinceLastRender = currentTime - lastRenderTime_;
  if (throttleFps_ > 0
      && timeSinceLastRender > 0
      && timeSinceLastRender < ONE_SECOND / throttleFps_) {
    const Milliseconds delayDuration = ONE_SECOND / throttleFps_ - timeSinceLastRender;
    info("%u delaying for %u", currentTime, delayDuration);
    delay(delayDuration);
    currentTime = timeMillis();
    timeSinceLastRender = currentTime - lastRenderTime_;
  }

  if (currentTime - lastFpsProbeTime_ > ONE_SECOND) {
    fps_ = framesSinceFpsProbe_;
    lastFpsProbeTime_ = currentTime;
    framesSinceFpsProbe_ = 0;
  }
  lastRenderTime_ = currentTime;
  framesSinceFpsProbe_++;

  render(timeSinceLastRender, currentTime);
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
static constexpr Milliseconds kEffectDuration = 10 * ONE_SECOND;

void Player::render(Milliseconds timeSinceLastRender, Milliseconds currentTime) {
  if (!ready_) {
    begin();
  }

  if (!paused_) {
    elapsedTime_ += timeSinceLastRender;
  }
#if 0
  Milliseconds brd = ONE_MINUTE / tempo_;
  //Milliseconds timeSinceDownbeat = (currTime - lastDownbeatTime_) % brd;

  Milliseconds currEffectDuration = brd * int(kEffectDuration / brd);
#else
  Milliseconds currEffectDuration = kEffectDuration;
#endif

  if (elapsedTime_ > currEffectDuration && !loop_) {
    info("%u Exceeded effect duration, switching to next effect",
         currentTime);
    nextInner(currentTime);
  }

  static constexpr Milliseconds minLEDWriteTime = 10;
  if (lastLEDWriteTime_ >= 0 &&
      currentTime - minLEDWriteTime < lastLEDWriteTime_) {
    return;
  }
  lastLEDWriteTime_ = currentTime;

  const Effect* effect = currentEffect();

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

  Frame efr = effectFrame(effect, currentTime);
  effect->rewind(efr);

  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    unisparks::render(*s->layout, s->renderer, *effect, efr);
  }
}

void Player::nextInner(Milliseconds currentTime) {
  updateToNewPattern(computeNextPattern(currentPattern_),
                     /*elapsedTime=*/0, currentTime);
}

void Player::reactToUserInput(Milliseconds currentTime) {
  lastUserInputTime_ = currentTime;
  followingLeader_ = false;
  if (network_) {
    network_->triggerSendAsap(currentTime);
  }
#if ESP32_BLE_SENDER
  Esp32BleNetwork::get()->triggerSendAsap(currentTime);
#endif // ESP32_BLE_SENDER
}

void Player::next(Milliseconds currentTime) {
  nextInner(currentTime);
  reactToUserInput(currentTime);
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

Precedence Player::GetFollowedPrecedence(Milliseconds currentTime) {
if (followingLeader_) {
    return DepreciatePrecedence(GetLeaderPrecedence(currentTime), 100);
  } else {
    return GetLocalPrecedence(currentTime);
  }
}

NetworkDeviceId Player::GetFollowedDeviceId() {
  if (followingLeader_) {
    return leaderDeviceId_;
  } else {
#if ESP32_BLE_SENDER
    return Esp32BleNetwork::get()->getLocalAddress();
#else // ESP32_BLE_SENDER
    return NetworkDeviceId(); // TODO
#endif // ESP32_BLE_SENDER
  }
}

void Player::updateToNewPattern(PatternBits newPattern,
                                Milliseconds elapsedTime,
                                Milliseconds currentTime) {
  elapsedTime_ = elapsedTime;
  if (newPattern == currentPattern_) {
    return;
  }
  currentPattern_ = newPattern;
  Effect* effect = currentEffect();
  info("%u Switching to pattern %s %s",
       currentTime,
       effect->name().c_str(),
       displayBitsAsBinary(currentPattern_).c_str());
  effect->begin(effectFrame(effect, currentTime));
  lastLEDWriteTime_ = -1;
#if ESP32_BLE_SENDER
  NetworkMessage message;
  message.originator = GetFollowedDeviceId();
  // TODO message.sender;
  message.currentPattern = currentPattern_;
  // TODO message.nextPattern;
  message.elapsedTime = elapsedTime;
  message.precedence = GetFollowedPrecedence(currentTime);
  info("%u Sending BLE as %s " DEVICE_ID_FMT " precedence %u",
        currentTime, (followingLeader_ ? "remote" : "local"),
        DEVICE_ID_HEX(message.originator), message.precedence);
  Esp32BleNetwork::get()->setMessageToSend(message, currentTime);
#endif // ESP32_BLE_SENDER
}

void Player::syncToNetwork(Milliseconds currentTime) {
  if (!network_) {
    return;
  }
  NetworkMessage messageToSend;
  messageToSend.originator = GetFollowedDeviceId();
  messageToSend.currentPattern = currentPattern_;
  messageToSend.nextPattern = 0; // TODO;
  messageToSend.elapsedTime = elapsedTime_;
  messageToSend.precedence = GetFollowedPrecedence(currentTime);
  network_->setMessageToSend(messageToSend, currentTime);
  network_->runLoop(currentTime);
  std::list<NetworkMessage> receivedMessages = network_->getReceivedMessages(currentTime);
  for (NetworkMessage receivedMessage : receivedMessages) {
    lastLEDWriteTime_ = -1;
    updateToNewPattern(receivedMessage.currentPattern,
                       receivedMessage.elapsedTime,
                       currentTime);
  }
#if ESP32_BLE_RECEIVER
  for (NetworkMessage message :
    Esp32BleNetwork::get()->getReceivedMessages(currentTime)) {
    const Milliseconds timeSinceReceipt = currentTime - message.receiptTime;
    if (message.elapsedTime < 0xFFFFFFFF - timeSinceReceipt) {
      message.elapsedTime += timeSinceReceipt;
    } else {
      message.elapsedTime = 0xFFFFFFFF;
    }
    NetworkDeviceId followedDeviceId = Esp32BleNetwork::get()->getLocalAddress();
    if (message.originator == followedDeviceId) {
      info("%u Ignoring received BLE from ourselves " DEVICE_ID_FMT,
           currentTime, DEVICE_ID_HEX(message.originator));
      continue;
    }
    Precedence followedPrecedence;
    if (followingLeader_) {
      followedDeviceId = leaderDeviceId_;
      followedPrecedence = GetLeaderPrecedence(currentTime);
    } else {
      followedPrecedence = GetLocalPrecedence(currentTime);
    }
    if (message.elapsedTime > kEffectDuration && !loop_) {
      // Ignore this message because the sender already switched effects.
      info("%u Ignoring received BLE with time %u past duration %u",
           currentTime, message.elapsedTime, kEffectDuration);
      continue;
    }
    if (comparePrecedence(followedPrecedence, followedDeviceId,
                          message.precedence, message.originator) >= 0) {
      info("%u Ignoring received BLE from " DEVICE_ID_FMT
           " precedence %u because our %s " DEVICE_ID_FMT " precedence %u is higher",
           currentTime, DEVICE_ID_HEX(message.originator), message.precedence,
           (followingLeader_ ? "remote" : "local"),
           DEVICE_ID_HEX(followedDeviceId), followedPrecedence);
      continue;
    }
    if (!followingLeader_) {
      info("%u Switching from local " DEVICE_ID_FMT " to leader"
           " " DEVICE_ID_FMT " precedence %u prior precedence %u",
           currentTime, DEVICE_ID_HEX(followedDeviceId),
           DEVICE_ID_HEX(message.originator),
           message.precedence, followedPrecedence);
    } else if (leaderDeviceId_ != message.originator) {
      info("%u Picking new leader " DEVICE_ID_FMT " precedence %u"
           " over " DEVICE_ID_FMT " precedence %u",
           currentTime, DEVICE_ID_HEX(message.originator), message.precedence,
           DEVICE_ID_HEX(leaderDeviceId_), followedPrecedence);
    } else {
      info("%u Keeping leader " DEVICE_ID_FMT " precedence %u"
           " prior precedence %u",
           currentTime, DEVICE_ID_HEX(message.originator), message.precedence,
           followedPrecedence);
    }
    followingLeader_ = true;
    leaderDeviceId_ = message.originator;
    leaderPrecedence_ = message.precedence;
    lastLeaderReceiveTime_ = currentTime;
    info("%u Received BLE: switching to pattern %s %s elapsedTime %u",
         currentTime, patternFromBits(message.currentPattern)->name().c_str(),
         displayBitsAsBinary(message.currentPattern).c_str(),
         message.elapsedTime);
    lastLEDWriteTime_ = -1;
    updateToNewPattern(message.currentPattern, message.elapsedTime, currentTime);
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

Frame Player::effectFrame(const Effect* effect, Milliseconds currentTime) {
  // Ensure effectContext_ is big enough for this effect.
  const size_t effectContextSize = effect->contextSize({viewport_, nullptr});
  if (effectContextSize > effectContextSize_) {
    info("%u realloc context from %zu to %zu",
         currentTime, effectContextSize_, effectContextSize);
    effectContextSize_ = effectContextSize;
    effectContext_ = realloc(effectContext_, effectContextSize_);
  }

  Frame frame;
  frame.animation.viewport = viewport_;
  frame.animation.context = effectContext_;
  frame.time = elapsedTime_;
  frame.tempo = tempo_;
  frame.metre = metre_;
  return frame;
}

const char* Player::command(const char* req) {
  static char res[256];
  const size_t MAX_CMD_LEN = 16;
  bool responded = false;

  if (!strncmp(req, "status?", MAX_CMD_LEN)) {
    // do nothing
  } else  if (!strncmp(req, "next", MAX_CMD_LEN)) {
    next(timeMillis());
  } else {
    snprintf(res, sizeof(res), "! unknown command");
    responded = true;
  }
  if (!responded) {
    snprintf(res, sizeof(res), "play %s %d",
             currentEffect()->name().c_str(), elapsedTime_);
  }
  debug("[%s] -> [%s]", req, res);
  return res;
}

} // namespace unisparks
