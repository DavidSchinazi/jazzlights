#include "unisparks/player.hpp"

#include <cstdio>
#include <limits>
#include <stdlib.h>
#include <assert.h>
#include <sstream>
#include "unisparks/effects/flame.hpp"
#include "unisparks/effects/glitter.hpp"
#include "unisparks/effects/glow.hpp"
#include "unisparks/effects/plasma.hpp"
#include "unisparks/effects/rainbow.hpp"
#include "unisparks/effects/solid.hpp"
#include "unisparks/effects/threesine.hpp"
#include "unisparks/pseudorandom.h"
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
auto purple_effect = solid(PURPLE, "purple");
auto cyan_effect = solid(CYAN, "cyan");
auto yellow_effect = solid(YELLOW, "yellow");
auto white_effect = solid(WHITE, "white");

auto red_glow_effect = glow(RED, "glow-red");
auto green_glow_effect = glow(GREEN, "glow-green");
auto blue_glow_effect = glow(BLUE, "glow-blue");
auto purple_glow_effect = glow(PURPLE, "glow-purple");
auto cyan_glow_effect = glow(CYAN, "glow-cyan");
auto yellow_glow_effect = glow(YELLOW, "glow-yellow");
auto white_glow_effect = glow(WHITE, "glow-white");

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

auto spin_rainbow_pattern = clone(SpinPlasma(OCPrainbow));
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
    const uint8_t byte1 = (pattern >> 24) & 0xFF;
    const uint8_t byte2 = (pattern >> 16) & 0xFF;
    const uint8_t byte3 = (pattern >>  8) & 0xFF;
    if (byte1 == 0 && byte2 == 0) {
      switch (byte3) {
        case 0x00: return &black_effect;
        case 0x01: return &red_effect;
        case 0x02: return &green_effect;
        case 0x03: return &blue_effect;
        case 0x04: return &purple_effect;
        case 0x05: return &cyan_effect;
        case 0x06: return &yellow_effect;
        case 0x07: return &white_effect;
        case 0x08: return &red_glow_effect;
        case 0x09: return &green_glow_effect;
        case 0x0A: return &blue_glow_effect;
        case 0x0B: return &purple_glow_effect;
        case 0x0C: return &cyan_glow_effect;
        case 0x0D: return &yellow_glow_effect;
        case 0x0E: return &white_glow_effect;
        case 0x0F: return &synctest;
#if WEARABLE
        case 0x10: return &calibration_effect;
#endif  // WEARABLE
      }
    }
    return &red_effect;
  } else {
    if (patternbit(pattern, 1)) { // spin
      if (patternbit(pattern, 2)) { // nature
        if (patternbit(pattern, 3)) { // rainbow
          return &spin_rainbow_pattern;
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
#if WEARABLE
          return &flame_pattern;
#else  // WEARABLE
          // TODO figure out why flame does not work on vehicles.
          return &spin_lava_pattern;
#endif  // WEARABLE
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
  viewport_.origin.x = 0;
  viewport_.origin.y = 0;
  viewport_.size.height = 1;
  viewport_.size.width = 1;
  frame_.player = this;
}

Player::~Player() {
  free(effectContext_);
  effectContext_ = nullptr;
  effectContextSize_ = 0;
}

Player& Player::clearStrands() {
  strandCount_ = 0;
  return *this;
}

Player& Player::addStrand(const Layout& l, SimpleRenderFunc r) {
  return addStrand(l, make<SimpleRenderer>(r));
}

Player& Player::addStrand(const Layout& l, Renderer& r) {
  constexpr size_t MAX_STRANDS = sizeof(strands_) / sizeof(*strands_);
  if (strandCount_ >= MAX_STRANDS) {
    fatal("Trying to add too many strands, max=%d", MAX_STRANDS);
  }
  strands_[strandCount_++] = {&l, &r};
  return *this;
}

Player& Player::connect(Network* n) {
  info("Connecting network %s", n->name());
  networks_.push_back(n);
  ready_ = false;
  return *this;
}

void Player::begin(Milliseconds currentTime) {
  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    viewport_ = merge(viewport_, unisparks::bounds(*s->layout));
  }

  int pxcnt = 0;
  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    pxcnt += s->layout->pixelCount();
  }

  // Figure out localDeviceId_.
  if (!randomizeLocalDeviceId_) {
    for (Network* network : networks_) {
      NetworkDeviceId localDeviceId = network->getLocalDeviceId();
      if (localDeviceId != NetworkDeviceId()) {
        localDeviceId_ = localDeviceId;
        break;
      }
    }
  }
  while (localDeviceId_ == NetworkDeviceId()) {
    // If no interfaces have a localDeviceId, generate one randomly.
    uint8_t deviceIdBytes[6] = {};
    UnpredictableRandom::GetBytes(&deviceIdBytes[0], sizeof(deviceIdBytes));
    localDeviceId_ = NetworkDeviceId(deviceIdBytes);
  }
  currentLeader_ = localDeviceId_;
  info("%u Starting Unisparks player %s (v%s); strands: %d%s, "
       "pixels: %d, %s " DEVICE_ID_FMT " w %f h %f ox %f oy %f",
       currentTime,
       BOOT_MESSAGE,
       UNISPARKS_VERSION,
       strandCount_,
       strandCount_ < 1 ? " (CONTROLLER ONLY!)" : "",
       pxcnt,
       !networks_.empty() ? "networked" : "standalone",
       DEVICE_ID_HEX(localDeviceId_),
       viewport_.size.width, viewport_.size.height,
       viewport_.origin.x, viewport_.origin.y);

  ready_ = true;

  currentPatternStartTime_ = currentTime;
  currentPattern_ = 0x12345678;
  nextPattern_ = computeNextPattern(currentPattern_);
}

void Player::handleSpecial() {
    static constexpr PatternBits kSpecialPatternBits[] = {
#if WEARABLE
    0x00001000,  // calibration.
#endif  // WEARABLE
    0x00000000,  // black.
    0x00000100,  // red.
    0x00000200,  // green.
    0x00000300,  // blue.
    0x00000700,  // white.
  };
  specialMode_++;
  if (specialMode_ > sizeof(kSpecialPatternBits) / sizeof(kSpecialPatternBits[0])) {
    specialMode_ = 1;
  }
  currentPattern_ = kSpecialPatternBits[specialMode_ - 1];
  nextPattern_ = currentPattern_;
  loop_ = true;
  info("Starting special mode %u", specialMode_);
}

void Player::stopSpecial() {
  if (specialMode_ == 0) {
    return;
  }
  info("Stopping special mode");
  specialMode_ = 0;
  loop_ = false;
  currentPattern_ = computeNextPattern(currentPattern_);
  nextPattern_ = computeNextPattern(currentPattern_);
}

void Player::render(Milliseconds currentTime) {
  if (!ready_) {
    begin(currentTime);
  }

  // First listen on all networks.
  for (Network* network : networks_) {
    for (NetworkMessage receivedMessage :
        network->getReceivedMessages(currentTime)) {
      handleReceivedMessage(receivedMessage, currentTime);
    }
  }

  // Then react to any received packets.
  checkLeaderAndPattern(currentTime);

  // Then give all networks the opportunity to send.
  for (Network* network : networks_) {
    network->runLoop(currentTime);
  }

  frame_.animation.viewport = viewport_;
  if (currentTime - currentPatternStartTime_ > kEffectDuration) {
    frame_.pattern = nextPattern_;
    frame_.time = currentTime - currentPatternStartTime_ - kEffectDuration;
  } else {
    frame_.pattern = currentPattern_;
    frame_.time = currentTime - currentPatternStartTime_;
  }
  Effect* effect = patternFromBits(frame_.pattern);

  // Ensure effectContext_ is big enough for this effect.
  const size_t effectContextSize = effect->contextSize({viewport_, nullptr});
  if (effectContextSize > effectContextSize_) {
    info("%u realloc context size from %zu to %zu (%s w %f h %f)",
         currentTime, effectContextSize_, effectContextSize,
         effect->name().c_str(), viewport_.size.width, viewport_.size.height);
    effectContextSize_ = effectContextSize;
    effectContext_ = realloc(effectContext_, effectContextSize_);
  }
  frame_.animation.context = effectContext_;

  if (effect != lastBegunEffect_) {
    lastBegunEffect_ = effect;
    randomInitialized_ = false;
    effect->begin(frame_);
    lastLEDWriteTime_ =1;
  }

  // Keep track of how many FPS we might be able to get.
  if (currentTime - lastFpsProbeTime_ > ONE_SECOND) {
    fps_ = framesSinceFpsProbe_;
    lastFpsProbeTime_ = currentTime;
    framesSinceFpsProbe_ = 0;
  }
  framesSinceFpsProbe_++;

  // Do not send data to LEDs faster than 100Hz.
  static constexpr Milliseconds minLEDWriteTime = 10;
  if (lastLEDWriteTime_ >= 0 &&
      currentTime - minLEDWriteTime < lastLEDWriteTime_) {
    return;
  }
  lastLEDWriteTime_ = currentTime;

  // Actually render the pixels.
  randomInitialized_ = false;
  effect->rewind(frame_);
  for (Strand* s = strands_;
       s < strands_ + strandCount_; ++s) {
    unisparks::render(*s->layout, s->renderer, *effect, frame_);
  }
}

void Player::next(Milliseconds currentTime) {
  info("%u next command received: switching from %s to %s, currentLeader=" DEVICE_ID_FMT,
        currentTime, patternFromBits(currentPattern_)->name().c_str(), patternFromBits(nextPattern_)->name().c_str(),
        DEVICE_ID_HEX(currentLeader_));
  lastUserInputTime_ = currentTime;
  currentPatternStartTime_ = currentTime;
  currentPattern_ = nextPattern_;
  nextPattern_ = computeNextPattern(nextPattern_);
  checkLeaderAndPattern(currentTime);
  info("%u next command processed: now current %s next %s, currentLeader=" DEVICE_ID_FMT,
        currentTime, patternFromBits(currentPattern_)->name().c_str(), patternFromBits(nextPattern_)->name().c_str(),
        DEVICE_ID_HEX(currentLeader_));

  for (Network* network : networks_) {
    network->triggerSendAsap(currentTime);
  }
}

Precedence getPrecedenceGain(Milliseconds epochTime,
                             Milliseconds currentTime,
                             Milliseconds duration,
                             Precedence maxGain) {
  if (epochTime < 0) {
    return 0;
  } else if (currentTime < epochTime) {
    return maxGain;
  } else if (currentTime - epochTime > duration) {
    return 0;
  }
  const Milliseconds timeDelta = currentTime - epochTime;
  if (timeDelta < duration / 10) {
    return maxGain;
  }
  return static_cast<uint64_t>(duration - timeDelta) * maxGain / duration;
}

Precedence addPrecedenceGain(Precedence startPrecedence,
                             Precedence gain) {
  if (startPrecedence >= std::numeric_limits<Precedence>::max() - gain) {
    return std::numeric_limits<Precedence>::max();
  }
  return startPrecedence + gain;
}

static constexpr Milliseconds kInputDuration = 10 * 60 * 1000;  // 10min.

Precedence Player::getLocalPrecedence(Milliseconds currentTime) {
  return addPrecedenceGain(basePrecedence_,
                           getPrecedenceGain(lastUserInputTime_, currentTime,
                                             kInputDuration, precedenceGain_));
}

Player::OriginatorEntry* Player::getOriginatorEntry(NetworkDeviceId originator,
                                                    Milliseconds /*currentTime*/) {
  OriginatorEntry* entry = nullptr;
  for (OriginatorEntry& e : originatorEntries_) {
    if (e.originator == originator) {
      return &e;
    }
  }
  return entry;
}

static constexpr Milliseconds kOriginationTimeOverride = 6000;
static constexpr Milliseconds kOriginationTimeDiscard = 9000;

static_assert(kOriginationTimeOverride < kOriginationTimeDiscard,
  "Inverting these can lead to retracting an originator "
  "while disallowing picking a replacement.");
static_assert(kOriginationTimeDiscard < kEffectDuration,
  "Inverting these can lead to keeping an originator "
  "past the end of its intended next pattern.");

void Player::checkLeaderAndPattern(Milliseconds currentTime) {
  // Remove elements that have aged out.
  originatorEntries_.remove_if([currentTime](const OriginatorEntry& e){
    if (currentTime > e.lastOriginationTime + kOriginationTimeDiscard) {
       info("%u Removing " DEVICE_ID_FMT "p%u entry due to origination time",
             currentTime, DEVICE_ID_HEX(e.originator), e.precedence);
      return true;
    }
    if (currentTime > e.currentPatternStartTime + 2 * kEffectDuration) {
       info("%u Removing " DEVICE_ID_FMT "p%u entry due to effect duration",
             currentTime, DEVICE_ID_HEX(e.originator), e.precedence);
      return true;
    }
    return false;
  });
  Precedence precedence = getLocalPrecedence(currentTime);
  NetworkDeviceId originator = localDeviceId_;
  const OriginatorEntry* entry = nullptr;
  const bool hadRecentUserInput =
    (lastUserInputTime_ >= 0 &&
      lastUserInputTime_ <= currentTime &&
      currentTime - lastUserInputTime_ < kInputDuration);
  // Keep ourselves as leader if there was recent user button input or if we are looping.
  if (!hadRecentUserInput && !loop_) {
    for (const OriginatorEntry& e : originatorEntries_) {
      if (e.retracted) {
        debug("%u ignoring " DEVICE_ID_FMT " due to retracted",
              currentTime, DEVICE_ID_HEX(e.originator));
        continue;
      }
      if (currentTime > e.lastOriginationTime + kOriginationTimeDiscard) {
        debug("%u ignoring " DEVICE_ID_FMT " due to origination time",
              currentTime, DEVICE_ID_HEX(e.originator));
        continue;
      }
      if (currentTime > e.currentPatternStartTime + 2 * kEffectDuration) {
        debug("%u ignoring " DEVICE_ID_FMT " due to effect duration",
              currentTime, DEVICE_ID_HEX(e.originator));
        continue;
      }
      if (comparePrecedence(e.precedence, e.originator,
                            precedence, originator) <= 0) {
        debug("%u ignoring " DEVICE_ID_FMT "p%u due to better " DEVICE_ID_FMT "p%u",
              currentTime, DEVICE_ID_HEX(e.originator), e.precedence,
              DEVICE_ID_HEX(originator), precedence);
        continue;
      }
      precedence = e.precedence;
      originator = e.originator;
      entry = &e;
    }
  }

  if (currentLeader_ != originator) {
    info("%u Switching leader from " DEVICE_ID_FMT " to " DEVICE_ID_FMT,
         currentTime, DEVICE_ID_HEX(currentLeader_),
         DEVICE_ID_HEX(originator));
    currentLeader_ = originator;
  }

  NumHops numHops;
  Milliseconds lastOriginationTime;
  Network* nextHopNetwork = nullptr;
  if (entry != nullptr) {
    // Update our state based on entry from leader.
    nextPattern_ = entry->nextPattern;
    currentPatternStartTime_ = entry->currentPatternStartTime;
    nextHopNetwork = entry->nextHopNetwork;
    numHops = entry->numHops;
    lastOriginationTime = entry->lastOriginationTime;
    if (currentPattern_ != entry->currentPattern) {
      currentPattern_ = entry->currentPattern;
      info("%u Following " DEVICE_ID_FMT "p%u nh=%u %s new currentPattern %s",
          currentTime, DEVICE_ID_HEX(originator), precedence, numHops, nextHopNetwork->name(),
          patternFromBits(currentPattern_)->name().c_str());
      lastLEDWriteTime_ = -1;
    }
  } else {
    // We are currently leading.
    numHops = 0;
    lastOriginationTime = currentTime;
    while (currentTime - currentPatternStartTime_ > kEffectDuration) {
      currentPatternStartTime_ += kEffectDuration;
      if (loop_) {
        nextPattern_ = currentPattern_;
      } else {
        currentPattern_ = nextPattern_;
        nextPattern_ = computeNextPattern(nextPattern_);
      }
      info("%u We (" DEVICE_ID_FMT "p%u) are leading, new currentPattern %s",
          currentTime, DEVICE_ID_HEX(localDeviceId_), precedence,
          patternFromBits(currentPattern_)->name().c_str());
      lastLEDWriteTime_ = -1;
    }
  }

  if (networks_.empty()) {
    debug("%u not setting messageToSend without networks", currentTime);
    return;
  }
  NetworkMessage messageToSend;
  messageToSend.originator = originator;
  messageToSend.sender = localDeviceId_;
  messageToSend.currentPattern = currentPattern_;
  messageToSend.nextPattern = nextPattern_;
  messageToSend.currentPatternStartTime = currentPatternStartTime_;
  messageToSend.precedence = precedence;
  messageToSend.lastOriginationTime = lastOriginationTime;
  messageToSend.numHops = numHops;
  for (Network* network : networks_) {
    if (!network->shouldEcho() && nextHopNetwork == network) {
      debug("%u Not echoing for %s to %s ",
            currentTime, network->name(),
            networkMessageToString(messageToSend, currentTime).c_str());
      network->disableSending(currentTime);
      continue;
    }
    debug("%u Setting messageToSend for %s to %s ",
          currentTime, network->name(),
          networkMessageToString(messageToSend, currentTime).c_str());
    network->setMessageToSend(messageToSend, currentTime);
  }
}

void Player::handleReceivedMessage(NetworkMessage message, Milliseconds currentTime) {
  debug("%u handleReceivedMessage %s",
        currentTime,
        networkMessageToString(message, currentTime).c_str());
  if (message.sender == localDeviceId_) {
    debug("%u Ignoring received message that we sent %s",
          currentTime, networkMessageToString(message, currentTime).c_str());
    return;
  }
  if (message.originator == localDeviceId_) {
    debug("%u Ignoring received message that we originated %s",
          currentTime, networkMessageToString(message, currentTime).c_str());
    return;
  }
  if (message.numHops == std::numeric_limits<NumHops>::max()) {
    // This avoids overflow when incrementing below.
    info("%u Ignoring received message with high numHops %s",
         currentTime, networkMessageToString(message, currentTime).c_str());
    return;
  }
  NumHops receiptNumHops = message.numHops + 1;
  if (currentTime > message.lastOriginationTime + kOriginationTimeDiscard) {
    info("%u Ignoring received message due to origination time %s",
          currentTime, networkMessageToString(message, currentTime).c_str());
    return;
  }
  if (currentTime > message.currentPatternStartTime + 2 * kEffectDuration) {
      info("%u Ignoring received message due to effect duration %s",
            currentTime, networkMessageToString(message, currentTime).c_str());
    return;
  }
  OriginatorEntry* entry = getOriginatorEntry(message.originator, currentTime);
  if (entry == nullptr) {
    originatorEntries_.push_back(OriginatorEntry());
    entry = &originatorEntries_.back();
    entry->originator = message.originator;
    entry->precedence = message.precedence;
    entry->currentPattern = message.currentPattern;
    entry->nextPattern = message.nextPattern;
    entry->currentPatternStartTime = message.currentPatternStartTime;
    entry->lastOriginationTime = message.lastOriginationTime;
    entry->nextHopDevice = message.sender;
    entry->nextHopNetwork = message.receiptNetwork;
    entry->numHops = receiptNumHops;
    entry->retracted = false;
    entry->patternStartTimeMovementCounter = 0;
    info("%u Adding " DEVICE_ID_FMT "p%u entry via " DEVICE_ID_FMT ".%s nh %u ot %u current %s next %s elapsed %u",
         currentTime, DEVICE_ID_HEX(entry->originator), entry->precedence,
         DEVICE_ID_HEX(entry->nextHopDevice), entry->nextHopNetwork->name(),
         entry->numHops, currentTime - entry->lastOriginationTime,
         patternFromBits(entry->currentPattern)->name().c_str(),
         patternFromBits(entry->nextPattern)->name().c_str(),
         currentTime - entry->currentPatternStartTime);
  } else {
    // The concept behind this is that we build a tree rooted at each originator
    // using a variant of the Bellman-Ford algorithm. We then only ever listen
    // to our next hop on the way to the originator to avoid oscillating between
    // neighbors. To avoid loops in this tree, we ignore any update that has same
    // or more hops than our currently saved one. To allow us to recover from
    // situations where the originator has moved further away in the network, we
    // accept those updates if they're more recent by kOriginationTimeOverride
    // than what we've seen so far. This is based on the theoretical points made
    // in Section 2 of RFC 8966 - we can say that while much simpler and less
    // powerful, this is inspired by the Babel Routing Protocol.
    if (entry->nextHopDevice != message.sender ||
        entry->nextHopNetwork != message.receiptNetwork) {
      bool changeNextHop = false;
      if (receiptNumHops < entry->numHops) {
        info("%u Switching " DEVICE_ID_FMT "p%u entry via " DEVICE_ID_FMT ".%s "
             "nh %u ot %u to better nextHop " DEVICE_ID_FMT ".%s nh %u ot %u due to nextHops",
             currentTime, DEVICE_ID_HEX(entry->originator), entry->precedence,
             DEVICE_ID_HEX(entry->nextHopDevice), entry->nextHopNetwork->name(),
             entry->numHops, currentTime - entry->lastOriginationTime,
             DEVICE_ID_HEX(message.sender), message.receiptNetwork->name(),
             receiptNumHops, currentTime - message.lastOriginationTime);
        changeNextHop = true;
      } else if (message.lastOriginationTime > entry->lastOriginationTime + kOriginationTimeOverride) {
        info("%u Switching " DEVICE_ID_FMT "p%u entry via " DEVICE_ID_FMT ".%s "
             "nh %u ot %u to better nextHop " DEVICE_ID_FMT ".%s nh %u ot %u due to originationTime",
             currentTime, DEVICE_ID_HEX(entry->originator), entry->precedence,
             DEVICE_ID_HEX(entry->nextHopDevice), entry->nextHopNetwork->name(),
             entry->numHops, currentTime - entry->lastOriginationTime,
             DEVICE_ID_HEX(message.sender), message.receiptNetwork->name(),
             receiptNumHops, currentTime - message.lastOriginationTime);
        changeNextHop = true;
      }
      if (changeNextHop) {
        entry->nextHopDevice = message.sender;
        entry->nextHopNetwork = message.receiptNetwork;
        entry->numHops = receiptNumHops;
      }
    }

    if (entry->nextHopDevice == message.sender &&
        entry->nextHopNetwork == message.receiptNetwork) {
      bool shouldUpdateStartTime = false;
      std::ostringstream changes;
      if (entry->precedence != message.precedence) {
        changes << ", precedence " << entry->precedence << " to " << message.precedence;
      }
      if (entry->currentPattern != message.currentPattern) {
        shouldUpdateStartTime = true;
        changes << ", currentPattern " << patternFromBits(entry->currentPattern)->name()
                << " to " << patternFromBits(message.currentPattern)->name();
      }
      if (entry->nextPattern != message.nextPattern) {
        shouldUpdateStartTime = true;
        changes << ", nextPattern " << patternFromBits(entry->nextPattern)->name()
                << " to " << patternFromBits(message.nextPattern)->name();
      }
      // Debounce incoming updates to currentPatternStartTime to avoid visual jitter in the presence
      // of network jitter.
      static constexpr Milliseconds kPatternStartTimeDeltaMin = 100;
      static constexpr Milliseconds kPatternStartTimeDeltaMax = 500;
      static constexpr int8_t kPatternStartTimeMovementThreshold = 5;
      if (entry->currentPatternStartTime > message.currentPatternStartTime) {
        const Milliseconds timeDelta = entry->currentPatternStartTime - message.currentPatternStartTime;
        if (shouldUpdateStartTime || timeDelta >= kPatternStartTimeDeltaMax) {
          changes << ", elapsedTime -= " << timeDelta;
          shouldUpdateStartTime = true;
        } else if (timeDelta < kPatternStartTimeDeltaMin) {
          if (is_debug_logging_enabled()) {
            changes << ", elapsedTime !-= " << timeDelta;
          }
          entry->patternStartTimeMovementCounter = 0;
        } else {
          if (entry->patternStartTimeMovementCounter <= 1) {
            entry->patternStartTimeMovementCounter--;
            if (entry->patternStartTimeMovementCounter <= -kPatternStartTimeMovementThreshold) {
              changes << ", elapsedTime -= " << timeDelta;
              shouldUpdateStartTime = true;
            } else {
              if (is_debug_logging_enabled()) {
                changes << ", elapsedTime ~-= " << timeDelta
                        << " (movement " << static_cast<int>(-entry->patternStartTimeMovementCounter) << ")";
              }
            }
          } else {
            entry->patternStartTimeMovementCounter = 0;
            if (is_debug_logging_enabled()) {
              changes << ", elapsedTime ~-= " << timeDelta << " (flip)";
            }
          }
        }
      } else if (entry->currentPatternStartTime < message.currentPatternStartTime) {
        const Milliseconds timeDelta = message.currentPatternStartTime - entry->currentPatternStartTime;
        if (shouldUpdateStartTime || timeDelta >= kPatternStartTimeDeltaMax) {
          changes << ", elapsedTime += " << timeDelta;
          if (entry->currentPattern == message.currentPattern && timeDelta >= kEffectDuration / 2) {
            changes << " (keeping currentPattern " << patternFromBits(entry->currentPattern)->name() << ")";
          }
          shouldUpdateStartTime = true;
        } else if (timeDelta < kPatternStartTimeDeltaMin) {
          if (is_debug_logging_enabled()) {
            changes << ", elapsedTime !+= " << timeDelta;
          }
          entry->patternStartTimeMovementCounter = 0;
        } else {
          if (entry->patternStartTimeMovementCounter >= -1) {
            entry->patternStartTimeMovementCounter++;
            if (entry->patternStartTimeMovementCounter >= kPatternStartTimeMovementThreshold) {
              changes << ", elapsedTime += " << timeDelta;
              shouldUpdateStartTime = true;
            } else {
              if (is_debug_logging_enabled()) {
                changes << ", elapsedTime ~+= " << timeDelta
                        << " (movement " << static_cast<int>(entry->patternStartTimeMovementCounter) << ")";
              }
            }
          } else {
            entry->patternStartTimeMovementCounter = 0;
            if (is_debug_logging_enabled()) {
              changes << ", elapsedTime ~+= " << timeDelta << " (flip)";
            }
          }
        }
      }
      if (entry->lastOriginationTime > message.lastOriginationTime) {
        changes << ", originationTime -= " << entry->lastOriginationTime - message.lastOriginationTime;
      } // Do not log increases to origination time since all originated messages cause it.
      if (entry->retracted) {
        changes << ", unretracted";
      }
      entry->precedence = message.precedence;
      entry->currentPattern = message.currentPattern;
      entry->nextPattern = message.nextPattern;
      entry->lastOriginationTime = message.lastOriginationTime;
      entry->retracted = false;
      if (shouldUpdateStartTime) {
        entry->currentPatternStartTime = message.currentPatternStartTime;
        entry->patternStartTimeMovementCounter = 0;
      }
      std::string changesStr = changes.str();
      if (!changesStr.empty()) {
        info("%u Accepting %s update from " DEVICE_ID_FMT "p%u via " DEVICE_ID_FMT ".%s%s%s",
            currentTime, (entry->originator == currentLeader_ ? "followed" : "ignored"),
            DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(entry->nextHopDevice),
            entry->nextHopNetwork->name(), changesStr.c_str(), message.receiptDetails.c_str());
      }
    } else {
      debug("%u Rejecting %s update from " DEVICE_ID_FMT "p%u via "
            DEVICE_ID_FMT ".%s because we are following " DEVICE_ID_FMT ".%s",
            currentTime, (entry->originator == currentLeader_ ? "followed" : "ignored"),
            DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(message.sender),
            message.receiptNetwork->name(), DEVICE_ID_HEX(entry->nextHopDevice),
            entry->nextHopNetwork->name());
    }
  }
  // If this sender is following another originator from what we previously heard,
  // retract any previous entries from them.
  for (OriginatorEntry& e : originatorEntries_) {
    if (e.nextHopDevice == message.sender &&
        e.nextHopNetwork == message.receiptNetwork &&
        e.originator != message.originator &&
        !e.retracted) {
      e.retracted = true;
      info("%u Retracting entry for originator " DEVICE_ID_FMT "p%u"
           " due to abandonment from " DEVICE_ID_FMT ".%s"
           " in favor of " DEVICE_ID_FMT "p%u",
           currentTime, DEVICE_ID_HEX(e.originator), e.precedence,
           DEVICE_ID_HEX(message.sender), message.receiptNetwork->name(),
           DEVICE_ID_HEX(message.originator), message.precedence);
    }
  }

  lastLEDWriteTime_ = -1;
}

void Player::loopOne(Milliseconds currentTime) {
  if (loop_) {
    return;
  }
  info("%u Looping", currentTime);
  loop_ = true;
  nextPattern_ = currentPattern_;
}

const char* Player::command(const char* req) {
  static char res[256];
  const size_t MAX_CMD_LEN = 16;
  bool responded = false;

  if (!strncmp(req, "status?", MAX_CMD_LEN)) {
    // do nothing
  } else  if (!strncmp(req, "next", MAX_CMD_LEN)) {
    next(timeMillis());
  } else  if (!strncmp(req, "prev", MAX_CMD_LEN)) {
    loopOne(timeMillis());
  } else {
    snprintf(res, sizeof(res), "! unknown command");
    responded = true;
  }
  if (!responded) {
    // This is used by the WebUI to display the current pattern name.
    snprintf(res, sizeof(res), "playing %s",
             lastBegunEffect_->name().c_str());
  }
  debug("[%s] -> [%s]", req, res);
  return res;
}

PredictableRandom& Player::predictableRandom() {
  if (!randomInitialized_) {
    predictableRandom_.ResetWithFrame(frame_, lastBegunEffect_->name().c_str());
    randomInitialized_ = true;
  }
  return predictableRandom_;
}

} // namespace unisparks
