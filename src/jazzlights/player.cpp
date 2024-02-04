#include "jazzlights/player.h"

#include <assert.h>
#include <stdlib.h>

#include <cstdio>
#include <limits>
#include <set>
#include <sstream>

#include "jazzlights/effects/calibration.h"
#include "jazzlights/effects/colored_bursts.h"
#include "jazzlights/effects/fairy_wand.h"
#include "jazzlights/effects/flame.h"
#include "jazzlights/effects/follow_strand.h"
#include "jazzlights/effects/glitter.h"
#include "jazzlights/effects/glow.h"
#include "jazzlights/effects/hiphotic.h"
#include "jazzlights/effects/mapping.h"
#include "jazzlights/effects/metaballs.h"
#include "jazzlights/effects/plasma.h"
#include "jazzlights/effects/rainbow.h"
#include "jazzlights/effects/solid.h"
#include "jazzlights/effects/sync_test.h"
#include "jazzlights/effects/the_matrix.h"
#include "jazzlights/effects/threesine.h"
#include "jazzlights/instrumentation.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/math.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

int comparePrecedence(Precedence leftPrecedence, const NetworkDeviceId& leftDeviceId, Precedence rightPrecedence,
                      const NetworkDeviceId& rightDeviceId) {
  if (leftPrecedence < rightPrecedence) {
    return -1;
  } else if (leftPrecedence > rightPrecedence) {
    return 1;
  }
  return leftDeviceId.compare(rightDeviceId);
}

constexpr bool patternIsReserved(PatternBits pattern) {
  // Patterns with lowest byte zero are reserved.
  return (pattern & 0xFF) == 0;
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

PatternBits applyPalette(PatternBits pattern, uint8_t palette) {
  // Avoid any reserved patterns.
  while (patternIsReserved(pattern)) { pattern = computeNextPattern(pattern); }
  // Set palette bit.
  pattern |= 0x80000000;
  // Clear palette.
  pattern &= 0xFFFF1FFF;
  // Set palette.
  pattern |= palette << 13;
  return pattern;
}

static const Effect* patternFromBits(PatternBits pattern) {
  // Static definitions of all patterns.
  static const SpinPlasma spin_pattern;
  static const Hiphotic hiphotic_pattern;
  static const Metaballs metaballs_pattern;
  static const ColoredBursts colored_bursts_pattern;
  static const Flame flame_pattern;
  static const Glitter glitter_pattern;
  static const TheMatrix thematrix_pattern;
  static const Rainbow rainbow_pattern;
  static const FunctionalEffect threesine_pattern = threesine();
  static const FunctionalEffect follow_strand_effect = follow_strand();
  static const FunctionalEffect mapping_effect = mapping();
  static const FunctionalEffect calibration_effect = calibration();
  static const FunctionalEffect sync_test_effect = sync_test();
  static const FunctionalEffect black_effect = solid(BLACK, "black");
  static const FunctionalEffect red_effect = solid(RED, "red");
  static const FunctionalEffect green_effect = solid(GREEN, "green");
  static const FunctionalEffect blue_effect = solid(BLUE, "blue");
  static const FunctionalEffect purple_effect = solid(PURPLE, "purple");
  static const FunctionalEffect cyan_effect = solid(CYAN, "cyan");
  static const FunctionalEffect yellow_effect = solid(YELLOW, "yellow");
  static const FunctionalEffect white_effect = solid(WHITE, "white");
  static const FunctionalEffect red_glow_effect = glow(RED, "glow-red");
  static const FunctionalEffect green_glow_effect = glow(GREEN, "glow-green");
  static const FunctionalEffect blue_glow_effect = glow(BLUE, "glow-blue");
  static const FunctionalEffect purple_glow_effect = glow(PURPLE, "glow-purple");
  static const FunctionalEffect cyan_glow_effect = glow(CYAN, "glow-cyan");
  static const FunctionalEffect yellow_glow_effect = glow(YELLOW, "glow-yellow");
  static const FunctionalEffect white_glow_effect = glow(WHITE, "glow-white");

  // Pattern selection from bits.
  if (patternIsReserved(pattern)) {
    const uint8_t byte1 = (pattern >> 24) & 0xFF;
    const uint8_t byte2 = (pattern >> 16) & 0xFF;
    if (byte1 == 0x00) {
      switch (byte2) {
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
        case 0x0F: return &sync_test_effect;
        case 0x10: return &calibration_effect;
        case 0x11: return &follow_strand_effect;
        case 0xFF:
          // 0xFF is reserved for looping through all the patterns from a given palette. This is currently only used in
          // the Core2 UI, where the "all-palette" menu option passes the value 0x00FF0000 internally. That code
          // modifies this special value before passing it to the player, so we don't expect to ever see it here. If it
          // is received over the network, it will be treated like any other unknown reserved effect.
          break;
      }
    } else if (byte1 == 0x01) {
      return &mapping_effect;
    }
    return &red_effect;
  } else {
    if (patternbit(pattern, 1)) {      // 1x - palette
      if (patternbit(pattern, 2)) {    // 11x - palette waves
        if (patternbit(pattern, 3)) {  // 111x - spin
          return &spin_pattern;
        } else {  // 110x - hiphotic
          return &hiphotic_pattern;
        }
      } else {                         // 10x - palette balls
        if (patternbit(pattern, 3)) {  // 101x - metaballs
          return &metaballs_pattern;
        } else {  // 100x - colored bursts
          return &colored_bursts_pattern;
        }
      }
    } else {                           // 0x - custom
      if (patternbit(pattern, 2)) {    // 01x - sparkly
        if (patternbit(pattern, 3)) {  // 011x - flame
          return &flame_pattern;
        } else {  // 010x - glitter
          return &glitter_pattern;
        }
      } else {                           // 00x - shiny
        if (patternbit(pattern, 3)) {    // 001x - threesine & the-matrix
          if (patternbit(pattern, 4)) {  // 0011x - the-matrix
            return &thematrix_pattern;
          } else {  // 0010x - threesine
            return &threesine_pattern;
          }
        } else {  // 00x - rainbow
          return &rainbow_pattern;
        }
      }
    }
  }
  jll_fatal("Failed to pick an effect %s", displayBitsAsBinary(pattern).c_str());
}

std::string patternName(PatternBits pattern) { return patternFromBits(pattern)->effectName(pattern); }

Player::Player() {
  frame_.predictableRandom = &predictableRandom_;
  // Work around a heap corruption issue that causes an abort when increasing the size of the memory.
  effectContextSize_ = 1024;
  effectContext_ = aligned_alloc(kMaxStateAlignment, effectContextSize_);
  if (effectContext_ == nullptr) {
    jll_fatal("aligned_alloc(%zu, %zu) failed", kMaxStateAlignment, effectContextSize_);
  }
}

Player::~Player() {
  free(effectContext_);
  effectContext_ = nullptr;
  effectContextSize_ = 0;
}

Player& Player::addStrand(const Layout& l, Renderer& r) {
  strands_.push_back({l, r});
  return *this;
}

Player& Player::connect(Network* n) {
  jll_info("Connecting network %s", n->networkName());
  networks_.push_back(n);
  ready_ = false;
  return *this;
}

void Player::begin(Milliseconds currentTime) {
  xyIndexStore_.Reset();
  frame_.pixelCount = 0;
  frame_.viewport.origin.x = 0;
  frame_.viewport.origin.y = 0;
  frame_.viewport.size.height = 0;
  frame_.viewport.size.width = 0;
  for (const Strand& s : strands_) {
    frame_.viewport = merge(frame_.viewport, jazzlights::bounds(s.layout));
    frame_.pixelCount += s.layout.pixelCount();
    xyIndexStore_.IngestLayout(&s.layout);
  }
  xyIndexStore_.Finalize(frame_.viewport);
  frame_.xyIndexStore = &xyIndexStore_;

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
  jll_info(
      "%u Starting JazzLights player %s; "
      "basePrecedence %u precedenceGain %u strands: %zu%s, "
      "pixels: %zu, %s " DEVICE_ID_FMT " w %f h %f ox %f oy %f xv %zu yv %zu",
      currentTime, BOOT_MESSAGE, basePrecedence_, precedenceGain_, strands_.size(),
      strands_.empty() ? " (CONTROLLER ONLY!)" : "", frame_.pixelCount, !networks_.empty() ? "networked" : "standalone",
      DEVICE_ID_HEX(localDeviceId_), frame_.viewport.size.width, frame_.viewport.size.height, frame_.viewport.origin.x,
      frame_.viewport.origin.y, xyIndexStore_.xValuesCount(), xyIndexStore_.yValuesCount());

  ready_ = true;

  currentPatternStartTime_ = currentTime;
  currentPattern_ = enforceForcedPalette(0xef74ab26);
  nextPattern_ = enforceForcedPalette(computeNextPattern(currentPattern_));
#if defined(JL_START_SPECIAL) && JL_START_SPECIAL
  handleSpecial(currentTime);
#elif JL_IS_CONFIG(HAMMER)
  // Hammer defaults to looping glow-red pattern.
  currentPattern_ = 0x00080000;
  nextPattern_ = currentPattern_;
  loop_ = true;
#endif  // JL_START_SPECIAL
}

void Player::updatePrecedence(Precedence basePrecedence, Precedence precedenceGain, Milliseconds currentTime) {
  if (basePrecedence == basePrecedence_ && precedenceGain == precedenceGain_) { return; }
  basePrecedence_ = basePrecedence;
  precedenceGain_ = precedenceGain;
  jll_info("%u updating precedence to base %u gain %u", currentTime, basePrecedence, precedenceGain);
  if (!ready_) { return; }
  checkLeaderAndPattern(currentTime);
  for (Network* network : networks_) { network->triggerSendAsap(currentTime); }
}

void Player::handleSpecial(Milliseconds currentTime) {
  static constexpr PatternBits kSpecialPatternBits[] = {
      0x00100000,  // calibration.
      0x00000000,  // black.
      0x00010000,  // red.
      0x00020000,  // green.
      0x00030000,  // blue.
      0x00070000,  // white.
  };
  specialMode_++;
  if (specialMode_ > sizeof(kSpecialPatternBits) / sizeof(kSpecialPatternBits[0])) { specialMode_ = 1; }
  currentPattern_ = kSpecialPatternBits[specialMode_ - 1];
  nextPattern_ = currentPattern_;
  loop_ = true;
  jll_info("%u Starting special mode %zu", currentTime, specialMode_);
}

void Player::stopSpecial(Milliseconds currentTime) {
  if (specialMode_ == 0) { return; }
  jll_info("%u Stopping special mode", currentTime);
  specialMode_ = 0;
  currentPattern_ = enforceForcedPalette(computeNextPattern(currentPattern_));
  nextPattern_ = enforceForcedPalette(computeNextPattern(currentPattern_));
}

#if JL_IS_CONFIG(FAIRY_WAND)
void Player::triggerPatternOverride(Milliseconds currentTime) {
  jll_info("%u Triggering pattern override", currentTime);
  overridePatternStartTime_ = currentTime;
}
#endif  // FAIRY_WAND

bool Player::render(Milliseconds currentTime) {
  if (!ready_) { begin(currentTime); }

  // First listen on all networks.
  for (Network* network : networks_) {
    for (NetworkMessage receivedMessage : network->getReceivedMessages(currentTime)) {
      handleReceivedMessage(receivedMessage, currentTime);
    }
  }

  // Then react to any received packets.
  checkLeaderAndPattern(currentTime);

  // Then give all networks the opportunity to send.
  for (Network* network : networks_) { network->runLoop(currentTime); }

  frame_.context = nullptr;
  if (currentTime - currentPatternStartTime_ > kEffectDuration) {
    frame_.pattern = nextPattern_;
    frame_.time = currentTime - currentPatternStartTime_ - kEffectDuration;
  } else {
    frame_.pattern = currentPattern_;
    frame_.time = currentTime - currentPatternStartTime_;
  }
  const Effect* effect = patternFromBits(frame_.pattern);
#if JL_IS_CONFIG(FAIRY_WAND)
  constexpr Milliseconds kOverridePatternDuration = 8000;
  static const FunctionalEffect fairy_wand_effect = fairy_wand();
  if (overridePatternStartTime_ >= 0 && currentTime - overridePatternStartTime_ < kOverridePatternDuration) {
    frame_.time = currentTime - overridePatternStartTime_;
    effect = &fairy_wand_effect;
  }
#endif  // FAIRY_WAND

  // Ensure effectContext_ is big enough for this effect.
  size_t effectContextSize = effect->contextSize(frame_);
  if (effectContextSize > effectContextSize_) {
    if ((effectContextSize % kMaxStateAlignment) != 0) {
      // aligned_alloc required the allocation size to be a multiple of the alignment.
      effectContextSize += kMaxStateAlignment - (effectContextSize % kMaxStateAlignment);
    }
    jll_info("%u realloc context size from %zu to %zu (%s w %f h %f xv %zu yv %zu)", currentTime, effectContextSize_,
             effectContextSize, effect->effectName(frame_.pattern).c_str(), frame_.viewport.size.width,
             frame_.viewport.size.height, xyIndexStore_.xValuesCount(), xyIndexStore_.yValuesCount());
    // realloc doesn't support alignment requirements, so we need to use aligned_alloc and copy the data ourselves.
    size_t previousContextSize = effectContextSize_;
    void* previousContext = effectContext_;
    effectContextSize_ = effectContextSize;
    effectContext_ = aligned_alloc(kMaxStateAlignment, effectContextSize_);
    if (effectContext_ == nullptr) {
      jll_fatal("%u aligned_alloc(%zu, %zu) failed", currentTime, kMaxStateAlignment, effectContextSize_);
    }
    memcpy(effectContext_, previousContext, previousContextSize);
    free(previousContext);
  }
  frame_.context = effectContext_;

  if (frame_.pattern != lastBegunPattern_ || shouldBeginPattern_) {
    lastBegunPattern_ = frame_.pattern;
    shouldBeginPattern_ = false;
    predictableRandom_.ResetWithFrameStart(frame_, effect->effectName(frame_.pattern).c_str());
    effect->begin(frame_);
    lastLEDWriteTime_ = 1;
  }

  // Keep track of how many FPS we might be able to get.
  framesSinceFpsProbe_++;
  if (currentTime - lastFpsProbeTime_ > ONE_SECOND) {
    fps_ = framesSinceFpsProbe_ * 1000 / (currentTime - lastFpsProbeTime_);
    lastFpsProbeTime_ = currentTime;
    framesSinceFpsProbe_ = 0;
  }

  // Do not send data to LEDs faster than 100Hz.
  static constexpr Milliseconds minLEDWriteTime = 10;
  if (lastLEDWriteTime_ >= 0 && currentTime - minLEDWriteTime < lastLEDWriteTime_) { return false; }
  lastLEDWriteTime_ = currentTime;

  // Actually render the pixels.
  predictableRandom_.ResetWithFrameTime(frame_, effect->effectName(frame_.pattern).c_str());
  effect->rewind(frame_);
  Pixel px;
  for (const Strand& s : strands_) {
    px.layout = &s.layout;
    const size_t numPixels = s.layout.pixelCount();
    for (size_t index = 0; index < numPixels; index++) {
      Color color;
      px.coord = s.layout.at(index);
      if (!IsEmpty(px.coord)) {
        px.index = index;
        color = effect->color(frame_, px);
      } else {
        color = BLACK;
      }
      s.renderer.renderPixel(index, color);
    }
  }
  effect->afterColors(frame_);
  return true;
}

PatternBits Player::currentEffect() const { return lastBegunPattern_; }
std::string Player::currentEffectName() const { return patternName(lastBegunPattern_); }

void Player::next(Milliseconds currentTime) {
  jll_info("%u next command received: switching from %s (%4x) to %s (%4x), currentLeader=" DEVICE_ID_FMT, currentTime,
           patternName(currentPattern_).c_str(), currentPattern_, patternName(nextPattern_).c_str(), nextPattern_,
           DEVICE_ID_HEX(currentLeader_));
  lastUserInputTime_ = currentTime;
  currentPatternStartTime_ = currentTime;
  if (loop_ && currentPattern_ == nextPattern_) {
    currentPattern_ = enforceForcedPalette(computeNextPattern(currentPattern_));
    nextPattern_ = currentPattern_;
  } else {
    currentPattern_ = nextPattern_;
    nextPattern_ = enforceForcedPalette(computeNextPattern(nextPattern_));
  }
  checkLeaderAndPattern(currentTime);
  jll_info("%u next command processed: now current %s (%4x) next %s (%4x), currentLeader=" DEVICE_ID_FMT, currentTime,
           patternName(currentPattern_).c_str(), currentPattern_, patternName(nextPattern_).c_str(), nextPattern_,
           DEVICE_ID_HEX(currentLeader_));

  for (Network* network : networks_) { network->triggerSendAsap(currentTime); }
}

void Player::setPattern(PatternBits pattern, Milliseconds currentTime) {
  jll_info("%u set pattern command received: switching from %s (%4x) to %s (%4x), currentLeader=" DEVICE_ID_FMT,
           currentTime, patternName(currentPattern_).c_str(), currentPattern_, patternName(pattern).c_str(), pattern,
           DEVICE_ID_HEX(currentLeader_));
  lastUserInputTime_ = currentTime;
  currentPatternStartTime_ = currentTime;
  currentPattern_ = pattern;
  if (loop_ && currentPattern_ == nextPattern_) {
    nextPattern_ = currentPattern_;
  } else {
    nextPattern_ = enforceForcedPalette(computeNextPattern(pattern));
  }
  checkLeaderAndPattern(currentTime);
  jll_info("%u set pattern command processed: now current %s (%4x) next %s (%4x), currentLeader=" DEVICE_ID_FMT,
           currentTime, patternName(currentPattern_).c_str(), currentPattern_, patternName(nextPattern_).c_str(),
           nextPattern_, DEVICE_ID_HEX(currentLeader_));

  for (Network* network : networks_) { network->triggerSendAsap(currentTime); }
}

void Player::forcePalette(uint8_t palette, Milliseconds currentTime) {
  jll_info("%u Forcing palette %u", currentTime, palette);
  paletteIsForced_ = true;
  forcedPalette_ = palette;
  setPattern(enforceForcedPalette(currentPattern_), currentTime);
}

void Player::stopForcePalette(Milliseconds currentTime) {
  if (!paletteIsForced_) { return; }
  jll_info("%u Stop forcing palette %u", currentTime, forcedPalette_);
  paletteIsForced_ = false;
  forcedPalette_ = 0;
}

PatternBits Player::enforceForcedPalette(PatternBits pattern) {
  if (paletteIsForced_) { pattern = applyPalette(pattern, forcedPalette_); }
  return pattern;
}

Precedence getPrecedenceGain(Milliseconds epochTime, Milliseconds currentTime, Milliseconds duration,
                             Precedence maxGain) {
  if (epochTime < 0) {
    return 0;
  } else if (currentTime < epochTime) {
    return maxGain;
  } else if (currentTime - epochTime > duration) {
    return 0;
  }
  const Milliseconds timeDelta = currentTime - epochTime;
  if (timeDelta < duration / 10) { return maxGain; }
  return static_cast<uint64_t>(duration - timeDelta) * maxGain / duration;
}

Precedence addPrecedenceGain(Precedence startPrecedence, Precedence gain) {
  if (startPrecedence >= std::numeric_limits<Precedence>::max() - gain) {
    return std::numeric_limits<Precedence>::max();
  }
  return startPrecedence + gain;
}

static constexpr Milliseconds kInputDuration = 10 * 60 * 1000;  // 10min.

Precedence Player::getLocalPrecedence(Milliseconds currentTime) {
  return addPrecedenceGain(basePrecedence_,
                           getPrecedenceGain(lastUserInputTime_, currentTime, kInputDuration, precedenceGain_));
}

Player::OriginatorEntry* Player::getOriginatorEntry(NetworkDeviceId originator, Milliseconds /*currentTime*/) {
  OriginatorEntry* entry = nullptr;
  for (OriginatorEntry& e : originatorEntries_) {
    if (e.originator == originator) { return &e; }
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
  originatorEntries_.remove_if([currentTime](const OriginatorEntry& e) {
    if (currentTime > e.lastOriginationTime + kOriginationTimeDiscard) {
      jll_info("%u Removing " DEVICE_ID_FMT ".p%u entry due to origination time", currentTime,
               DEVICE_ID_HEX(e.originator), e.precedence);
      return true;
    }
    if (currentTime > e.currentPatternStartTime + 2 * kEffectDuration) {
      jll_info("%u Removing " DEVICE_ID_FMT ".p%u entry due to effect duration", currentTime,
               DEVICE_ID_HEX(e.originator), e.precedence);
      return true;
    }
    return false;
  });
  Precedence precedence = getLocalPrecedence(currentTime);
  NetworkDeviceId originator = localDeviceId_;
  const OriginatorEntry* entry = nullptr;
  const bool hadRecentUserInput = (lastUserInputTime_ >= 0 && lastUserInputTime_ <= currentTime &&
                                   currentTime - lastUserInputTime_ < kInputDuration);
  // Keep ourselves as leader if there was recent user button input or if we are looping.
  if (!hadRecentUserInput && !loop_) {
    for (const OriginatorEntry& e : originatorEntries_) {
      if (e.retracted) {
        jll_debug("%u ignoring " DEVICE_ID_FMT " due to retracted", currentTime, DEVICE_ID_HEX(e.originator));
        continue;
      }
      if (currentTime > e.lastOriginationTime + kOriginationTimeDiscard) {
        jll_debug("%u ignoring " DEVICE_ID_FMT " due to origination time", currentTime, DEVICE_ID_HEX(e.originator));
        continue;
      }
      if (currentTime > e.currentPatternStartTime + 2 * kEffectDuration) {
        jll_debug("%u ignoring " DEVICE_ID_FMT " due to effect duration", currentTime, DEVICE_ID_HEX(e.originator));
        continue;
      }
      if (comparePrecedence(e.precedence, e.originator, precedence, originator) <= 0) {
        jll_debug("%u ignoring " DEVICE_ID_FMT ".p%u due to better " DEVICE_ID_FMT ".p%u", currentTime,
                  DEVICE_ID_HEX(e.originator), e.precedence, DEVICE_ID_HEX(originator), precedence);
        continue;
      }
      precedence = e.precedence;
      originator = e.originator;
      entry = &e;
    }
  }

  if (currentLeader_ != originator) {
    jll_info("%u Switching leader from " DEVICE_ID_FMT " to " DEVICE_ID_FMT, currentTime, DEVICE_ID_HEX(currentLeader_),
             DEVICE_ID_HEX(originator));
    currentLeader_ = originator;
  }

  Milliseconds lastOriginationTime;
  if (entry != nullptr) {
    // Update our state based on entry from leader.
    nextPattern_ = entry->nextPattern;
    currentPatternStartTime_ = entry->currentPatternStartTime;
    followedNextHopNetwork_ = entry->nextHopNetwork;
    currentNumHops_ = entry->numHops;
    lastOriginationTime = entry->lastOriginationTime;
    if (currentPattern_ != entry->currentPattern) {
      currentPattern_ = entry->currentPattern;
      jll_info("%u Following " DEVICE_ID_FMT ".p%u nh=%u %s new currentPattern %s (%4x) %u FPS", currentTime,
               DEVICE_ID_HEX(originator), precedence, currentNumHops_,
               (followedNextHopNetwork_ != nullptr ? followedNextHopNetwork_->networkName() : "null"),
               patternName(currentPattern_).c_str(), currentPattern_, fps());
      printInstrumentationInfo(currentTime);
      lastLEDWriteTime_ = -1;
      shouldBeginPattern_ = true;
    }
  } else {
    // We are currently leading.
    followedNextHopNetwork_ = nullptr;
    currentNumHops_ = 0;
    lastOriginationTime = currentTime;
    while (currentTime - currentPatternStartTime_ > kEffectDuration) {
      currentPatternStartTime_ += kEffectDuration;
      if (loop_) {
        nextPattern_ = currentPattern_;
      } else {
        currentPattern_ = nextPattern_;
        nextPattern_ = enforceForcedPalette(computeNextPattern(nextPattern_));
      }
      jll_info("%u We (" DEVICE_ID_FMT ".p%u) are leading, new currentPattern %s (%4x) %u FPS", currentTime,
               DEVICE_ID_HEX(localDeviceId_), precedence, patternName(currentPattern_).c_str(), currentPattern_, fps());
      printInstrumentationInfo(currentTime);
      lastLEDWriteTime_ = -1;
      shouldBeginPattern_ = true;
    }
  }

  if (networks_.empty()) {
    jll_debug("%u not setting messageToSend without networks", currentTime);
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
  messageToSend.numHops = currentNumHops_;
  for (Network* network : networks_) {
    if (!network->shouldEcho() && followedNextHopNetwork_ == network) {
      jll_debug("%u Not echoing for %s to %s ", currentTime, network->networkName(),
                networkMessageToString(messageToSend, currentTime).c_str());
      network->disableSending(currentTime);
      continue;
    }
    jll_debug("%u Setting messageToSend for %s to %s ", currentTime, network->networkName(),
              networkMessageToString(messageToSend, currentTime).c_str());
    network->setMessageToSend(messageToSend, currentTime);
  }
}

void Player::handleReceivedMessage(NetworkMessage message, Milliseconds currentTime) {
  jll_debug("%u handleReceivedMessage %s", currentTime, networkMessageToString(message, currentTime).c_str());
  if (message.sender == localDeviceId_) {
    jll_debug("%u Ignoring received message that we sent %s", currentTime,
              networkMessageToString(message, currentTime).c_str());
    return;
  }
  if (message.originator == localDeviceId_) {
    jll_debug("%u Ignoring received message that we originated %s", currentTime,
              networkMessageToString(message, currentTime).c_str());
    return;
  }
  if (message.numHops == std::numeric_limits<NumHops>::max()) {
    // This avoids overflow when incrementing below.
    jll_info("%u Ignoring received message with high numHops %s", currentTime,
             networkMessageToString(message, currentTime).c_str());
    return;
  }
  NumHops receiptNumHops = message.numHops + 1;
  if (currentTime > message.lastOriginationTime + kOriginationTimeDiscard) {
    jll_info("%u Ignoring received message due to origination time %s", currentTime,
             networkMessageToString(message, currentTime).c_str());
    return;
  }
  if (currentTime > message.currentPatternStartTime + 2 * kEffectDuration) {
    jll_info("%u Ignoring received message due to effect duration %s", currentTime,
             networkMessageToString(message, currentTime).c_str());
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
    jll_info("%u Adding " DEVICE_ID_FMT ".p%u entry via " DEVICE_ID_FMT
             ".%s"
             " nh %u ot %u current %s (%4x) next %s (%4x) elapsed %u",
             currentTime, DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(entry->nextHopDevice),
             entry->nextHopNetwork->networkName(), entry->numHops, currentTime - entry->lastOriginationTime,
             patternName(entry->currentPattern).c_str(), entry->currentPattern, patternName(entry->nextPattern).c_str(),
             entry->nextPattern, currentTime - entry->currentPatternStartTime);
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
    if (entry->nextHopDevice != message.sender || entry->nextHopNetwork != message.receiptNetwork) {
      bool changeNextHop = false;
      if (receiptNumHops < entry->numHops) {
        jll_info("%u Switching " DEVICE_ID_FMT ".p%u entry via " DEVICE_ID_FMT
                 ".%s "
                 "nh %u ot %u to better nextHop " DEVICE_ID_FMT ".%s nh %u ot %u due to nextHops",
                 currentTime, DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(entry->nextHopDevice),
                 entry->nextHopNetwork->networkName(), entry->numHops, currentTime - entry->lastOriginationTime,
                 DEVICE_ID_HEX(message.sender), message.receiptNetwork->networkName(), receiptNumHops,
                 currentTime - message.lastOriginationTime);
        changeNextHop = true;
      } else if (message.lastOriginationTime > entry->lastOriginationTime + kOriginationTimeOverride) {
        jll_info("%u Switching " DEVICE_ID_FMT ".p%u entry via " DEVICE_ID_FMT
                 ".%s "
                 "nh %u ot %u to better nextHop " DEVICE_ID_FMT ".%s nh %u ot %u due to originationTime",
                 currentTime, DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(entry->nextHopDevice),
                 entry->nextHopNetwork->networkName(), entry->numHops, currentTime - entry->lastOriginationTime,
                 DEVICE_ID_HEX(message.sender), message.receiptNetwork->networkName(), receiptNumHops,
                 currentTime - message.lastOriginationTime);
        changeNextHop = true;
      }
      if (changeNextHop) {
        entry->nextHopDevice = message.sender;
        entry->nextHopNetwork = message.receiptNetwork;
        entry->numHops = receiptNumHops;
      }
    }

    if (entry->nextHopDevice == message.sender && entry->nextHopNetwork == message.receiptNetwork) {
      bool shouldUpdateStartTime = false;
      std::ostringstream changes;
      if (entry->precedence != message.precedence) {
        changes << ", precedence " << entry->precedence << " to " << message.precedence;
      }
      if (entry->currentPattern != message.currentPattern) {
        shouldUpdateStartTime = true;
        changes << ", currentPattern " << patternName(entry->currentPattern) << " to "
                << patternName(message.currentPattern);
      }
      if (entry->nextPattern != message.nextPattern) {
        shouldUpdateStartTime = true;
        changes << ", nextPattern " << patternName(entry->nextPattern) << " to " << patternName(message.nextPattern);
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
          if (is_debug_logging_enabled()) { changes << ", elapsedTime !-= " << timeDelta; }
          entry->patternStartTimeMovementCounter = 0;
        } else {
          if (entry->patternStartTimeMovementCounter <= 1) {
            entry->patternStartTimeMovementCounter--;
            if (entry->patternStartTimeMovementCounter <= -kPatternStartTimeMovementThreshold) {
              changes << ", elapsedTime -= " << timeDelta;
              shouldUpdateStartTime = true;
            } else {
              if (is_debug_logging_enabled()) {
                changes << ", elapsedTime ~-= " << timeDelta << " (movement "
                        << static_cast<int>(-entry->patternStartTimeMovementCounter) << ")";
              }
            }
          } else {
            entry->patternStartTimeMovementCounter = 0;
            if (is_debug_logging_enabled()) { changes << ", elapsedTime ~-= " << timeDelta << " (flip)"; }
          }
        }
      } else if (entry->currentPatternStartTime < message.currentPatternStartTime) {
        const Milliseconds timeDelta = message.currentPatternStartTime - entry->currentPatternStartTime;
        if (timeDelta > kEffectDuration - kEffectDuration / 10 && entry->originator == currentLeader_) {
          shouldBeginPattern_ = true;
        }
        if (shouldUpdateStartTime || timeDelta >= kPatternStartTimeDeltaMax) {
          changes << ", elapsedTime += " << timeDelta;
          if (entry->currentPattern == message.currentPattern && timeDelta >= kEffectDuration / 2) {
            changes << " (keeping currentPattern " << patternName(entry->currentPattern) << ")";
          }
          shouldUpdateStartTime = true;
        } else if (timeDelta < kPatternStartTimeDeltaMin) {
          if (is_debug_logging_enabled()) { changes << ", elapsedTime !+= " << timeDelta; }
          entry->patternStartTimeMovementCounter = 0;
        } else {
          if (entry->patternStartTimeMovementCounter >= -1) {
            entry->patternStartTimeMovementCounter++;
            if (entry->patternStartTimeMovementCounter >= kPatternStartTimeMovementThreshold) {
              changes << ", elapsedTime += " << timeDelta;
              shouldUpdateStartTime = true;
            } else {
              if (is_debug_logging_enabled()) {
                changes << ", elapsedTime ~+= " << timeDelta << " (movement "
                        << static_cast<int>(entry->patternStartTimeMovementCounter) << ")";
              }
            }
          } else {
            entry->patternStartTimeMovementCounter = 0;
            if (is_debug_logging_enabled()) { changes << ", elapsedTime ~+= " << timeDelta << " (flip)"; }
          }
        }
      }
      if (entry->lastOriginationTime > message.lastOriginationTime) {
        changes << ", originationTime -= " << entry->lastOriginationTime - message.lastOriginationTime;
      }  // Do not log increases to origination time since all originated messages cause it.
      if (entry->retracted) { changes << ", unretracted"; }
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
        const bool followedUpdate = entry->originator == currentLeader_;
        jll_info("%u Accepting %s update from " DEVICE_ID_FMT ".p%u via " DEVICE_ID_FMT ".%s%s%s", currentTime,
                 (followedUpdate ? "followed" : "ignored"), DEVICE_ID_HEX(entry->originator), entry->precedence,
                 DEVICE_ID_HEX(entry->nextHopDevice), entry->nextHopNetwork->networkName(), changesStr.c_str(),
                 message.receiptDetails.c_str());
        if (followedUpdate) { printInstrumentationInfo(currentTime); }
      }
    } else {
      jll_debug("%u Rejecting %s update from " DEVICE_ID_FMT ".p%u via " DEVICE_ID_FMT
                ".%s because we are following " DEVICE_ID_FMT ".%s",
                currentTime, (entry->originator == currentLeader_ ? "followed" : "ignored"),
                DEVICE_ID_HEX(entry->originator), entry->precedence, DEVICE_ID_HEX(message.sender),
                message.receiptNetwork->networkName(), DEVICE_ID_HEX(entry->nextHopDevice),
                entry->nextHopNetwork->networkName());
    }
  }
  // If this sender is following another originator from what we previously heard,
  // retract any previous entries from them.
  for (OriginatorEntry& e : originatorEntries_) {
    if (e.nextHopDevice == message.sender && e.nextHopNetwork == message.receiptNetwork &&
        e.originator != message.originator && !e.retracted) {
      e.retracted = true;
      jll_info("%u Retracting entry for originator " DEVICE_ID_FMT
               ".p%u"
               " due to abandonment from " DEVICE_ID_FMT
               ".%s"
               " in favor of " DEVICE_ID_FMT ".p%u",
               currentTime, DEVICE_ID_HEX(e.originator), e.precedence, DEVICE_ID_HEX(message.sender),
               message.receiptNetwork->networkName(), DEVICE_ID_HEX(message.originator), message.precedence);
    }
  }

  lastLEDWriteTime_ = -1;
}

void Player::loopOne(Milliseconds currentTime) {
  if (loop_) { return; }
  jll_info("%u Looping", currentTime);
  loop_ = true;
  nextPattern_ = currentPattern_;
}

void Player::stopLooping(Milliseconds currentTime) {
  if (!loop_) { return; }
  jll_info("%u Stopping loop", currentTime);
  loop_ = false;
  nextPattern_ = enforceForcedPalette(computeNextPattern(currentPattern_));
}

const char* Player::command(const char* req) {
  static char res[256];
  const size_t MAX_CMD_LEN = 16;
  bool responded = false;

  const Milliseconds currentTime = timeMillis();
  if (!strncmp(req, "status?", MAX_CMD_LEN)) {
    // do nothing
  } else if (!strncmp(req, "next", MAX_CMD_LEN)) {
    stopLooping(currentTime);
    next(currentTime);
  } else if (!strncmp(req, "prev", MAX_CMD_LEN)) {
    loopOne(currentTime);
  } else {
    snprintf(res, sizeof(res), "! unknown command");
    responded = true;
  }
  if (!responded) {
    // This is used by the WebUI to display the current pattern name.
    snprintf(res, sizeof(res), "playing %s", patternName(lastBegunPattern_).c_str());
  }
  jll_debug("[%s] -> [%s]", req, res);
  return res;
}

}  // namespace jazzlights
