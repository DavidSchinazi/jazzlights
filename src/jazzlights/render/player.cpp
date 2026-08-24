#include "jazzlights/render/player.h"

#include <assert.h>
#include <stdlib.h>

#include <cstdio>

#include "jazzlights/effect/calibration.h"
#include "jazzlights/effect/clouds.h"
#include "jazzlights/effect/colored_bursts.h"
#include "jazzlights/effect/creatures.h"
#include "jazzlights/effect/fairy_wand.h"
#include "jazzlights/effect/flame.h"
#include "jazzlights/effect/follow_strand.h"
#include "jazzlights/effect/glitter.h"
#include "jazzlights/effect/glow.h"
#include "jazzlights/effect/hiphotic.h"
#include "jazzlights/effect/mapping.h"
#include "jazzlights/effect/metaballs.h"
#include "jazzlights/effect/planet.h"
#include "jazzlights/effect/plasma.h"
#include "jazzlights/effect/rings.h"
#include "jazzlights/effect/solid.h"
#include "jazzlights/effect/sound_effect.h"
#include "jazzlights/effect/sync_test.h"
#include "jazzlights/effect/the_matrix.h"
#include "jazzlights/effect/threesine.h"
#include "jazzlights/protocol/engine.h"
#include "jazzlights/util/instrumentation.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

#if !defined(ESP32) || (JL_WIFI && !JL_ESP32_WIFI) || (JL_ETHERNET && !JL_ESP32_ETHERNET)
// The Arduino network variants only read from the the primary runloop, so the Player cannot sleep on it.
// The same is currently true for the non-ESP32 variants.
#define JL_PLAYER_SLEEPS 0
#else
#define JL_PLAYER_SLEEPS 1
#endif

#ifndef JL_PLAYER_SKIP_FLAME
#if JL_MOTOR
#define JL_PLAYER_SKIP_FLAME 1
#else  // JL_MOTOR
#define JL_PLAYER_SKIP_FLAME 0
#endif  // JL_MOTOR
#endif  // JL_PLAYER_SKIP_FLAME

#ifndef JL_PLAYER_LOG_MESSAGES
#define JL_PLAYER_LOG_MESSAGES 0
#endif  // JL_PLAYER_LOG_MESSAGES

#if JL_PLAYER_LOG_MESSAGES
#define jll_player_message(...) jll_info(__VA_ARGS__)
#else  // JL_PLAYER_LOG_MESSAGES
#define jll_player_message(...) jll_debug(__VA_ARGS__)
#endif  // JL_PLAYER_LOG_MESSAGES

namespace jazzlights {
namespace {
#if JL_IS_CONFIG(XMAS_TREE)
constexpr PatternBits kWarmPattern = 0x00001500;
#elif JL_IS_CONFIG(CREATURE)
constexpr PatternBits kCreaturePattern = 0x0000FF00;
#endif
}  // namespace

static constexpr CRGB warmColor() {
  // Based on the example values from:
  // https://www.usuallypragmatic.com/projects/Generating-Color-Temperature-Equivalent-Light-with-RGB-LEDs.html
  return CRGB(255, 67, 5);
}

static const Effect* patternFromBits(PatternBits pattern, const Player& player) {
  // Static definitions of all patterns.
  static const SpinPlasma spin_pattern;
  static const Hiphotic hiphotic_pattern;
  static const Metaballs metaballs_pattern;
  static const ColoredBursts colored_bursts_pattern;
  static const Flame flame_pattern;
  static const Glitter glitter_pattern;
  static const TheMatrix thematrix_pattern;
  static const Rings rings_pattern;
#if JL_AUDIO_VISUALIZER
  static const SoundEffect sound_effect;
#endif  // JL_AUDIO_VISUALIZER
  static const FunctionalEffect threesine_pattern = threesine();
  static const FunctionalEffect follow_strand_effect = follow_strand();
  static const FunctionalEffect mapping_effect = mapping();
  static const FunctionalEffect coloring_effect = coloring();
  static const FunctionalEffect calibration_effect = calibration();
  static const FunctionalEffect sync_test_effect = sync_test();
  static const FunctionalEffect black_effect = solid(CRGB::Black, "black");
  static const FunctionalEffect red_effect = solid(CRGB::Red, "red");
  static const FunctionalEffect green_effect = solid(CRGB::Green, "green");
  static const FunctionalEffect blue_effect = solid(CRGB::Blue, "blue");
  static const FunctionalEffect purple_effect = solid(CRGB::Purple, "purple");
  static const FunctionalEffect cyan_effect = solid(CRGB::Cyan, "cyan");
  static const FunctionalEffect yellow_effect = solid(CRGB::Yellow, "yellow");
  static const FunctionalEffect white_effect = solid(CRGB::White, "white");
  static const FunctionalEffect warm_effect = solid(warmColor(), "warm");
  static const FunctionalEffect red_glow_effect = glow(CRGB::Red, "glow-red");
  static const FunctionalEffect green_glow_effect = glow(CRGB::Green, "glow-green");
  static const FunctionalEffect blue_glow_effect = glow(CRGB::Blue, "glow-blue");
  static const FunctionalEffect purple_glow_effect = glow(CRGB::Purple, "glow-purple");
  static const FunctionalEffect cyan_glow_effect = glow(CRGB::Cyan, "glow-cyan");
  static const FunctionalEffect yellow_glow_effect = glow(CRGB::Yellow, "glow-yellow");
  static const FunctionalEffect white_glow_effect = glow(CRGB::White, "glow-white");
  static const FunctionalEffect warm_glow_effect = glow(warmColor(), "glow-warm");
#if JL_IS_CONFIG(CLOUDS)
  static const Clouds clouds_effect = Clouds();
#elif JL_IS_CONFIG(CREATURE)
  static const Creatures creatures_effect = Creatures();
#endif

  // Pattern selection from bits.
  // If the pattern bits have the four least-significant bits all zero then this is a reserved pattern,
  // and we examine the next four bits to determine what *type* of reserved pattern it is.
  // If the next four bits are also zero (reserved type zero),
  // then we examine the next eight bits to determine which particular one
  // of the reserved type zero patterns it is (generally simple solid colors).
  // Reserved types 0 (basic), 1 (mapping), and 2 (coloring) don’t use a palette.
  // Reserved type 3 and the non-reserved patterns do use ColorWithPalette.
  if (patternIsReserved(pattern)) {
    const uint8_t reserved_type = (pattern >> 4) & 0xF;
    if (reserved_type == 0x0) {
      switch ((pattern >> 8) & 0xFF) {
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
        case 0x12: return &glitter_pattern;
        case 0x13: return &thematrix_pattern;
        case 0x14: return &threesine_pattern;
        case 0x15: return &warm_effect;
        case 0x16: return &warm_glow_effect;
#if JL_IS_CONFIG(ORRERY_PLANET)
        case 0xFE: return PlanetEffect::Get();
#endif  // ORRERY_PLANET
        case 0xFF:
#if JL_IS_CONFIG(CREATURE)
          return &creatures_effect;
#else   // CREATURE
          return &white_glow_effect;
#endif  // CREATURE
      }
    } else if (reserved_type == 0x1) {
      return &mapping_effect;
    } else if (reserved_type == 0x2) {
      return &coloring_effect;
    } else if (reserved_type == 0x3) {
      // Reserved effects that use a palette.
      switch ((pattern >> 8) & 0xF) {
        case 0x0:  // Use the pattern bits.
          if (patternbit(pattern, 1)) {
            return &metaballs_pattern;
          } else {
            return &colored_bursts_pattern;
          }
          break;
        case 0xF:
          // 0xF is reserved for looping through all the patterns from a given palette. This is currently only used in
          // the Core2 UI, where the "all-palette" menu option passes the value kAllPalettePattern internally. That code
          // modifies this special value before passing it to the player, so we don't expect to ever see it here. If it
          // is received over the network, it will be treated like any other unknown reserved effect.
          break;
      }
    }
#if JL_IS_CONFIG(CLOUDS)
    else if (reserved_type == 0xF) {
      return &clouds_effect;
    }
#endif  // CLOUDS
    return &red_effect;
  } else {
#if JL_AUDIO_VISUALIZER
    if (player.sound_reactive_enabled()) { return &sound_effect; }
#else   // JL_AUDIO_VISUALIZER
    (void)player;
#endif  // JL_AUDIO_VISUALIZER
    if (patternbit(pattern, 1)) {
      if (patternbit(pattern, 2) && !player.isAllLinear()) {  // 11x - spin
        return &spin_pattern;
      } else {  // 10x - hiphotic
        return &hiphotic_pattern;
      }
    } else {
#if JL_PLAYER_SKIP_FLAME
      return &rings_pattern;
#else   // JL_PLAYER_SKIP_FLAME
      if (patternbit(pattern, 2) && !player.isAllLinear()) {  // 01x - flame
        return &flame_pattern;
      } else {  // 00x - rings
        return &rings_pattern;
      }
#endif  // JL_PLAYER_SKIP_FLAME
    }
  }
  jll_fatal("Failed to pick an effect %s", displayBitsAsBinary(pattern).c_str());
}

std::string patternName(PatternBits pattern, const Player& player) {
  return patternFromBits(pattern, player)->effectName(pattern);
}

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
  strands_.push_back({l, r, strands_.size()});
  return *this;
}

Player& Player::connect(Network* n) {
  jll_info("Connecting network %s", NetworkTypeToString(n->type()));
  networks_.push_back(n);
  engine_.SetHasNetworks(true);
  ready_ = false;
  return *this;
}

void Player::begin() {
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
  if (frame_.viewport.size.width == 0 || frame_.viewport.size.height == 0) { isAllLinear_ = true; }
  xyIndexStore_.Finalize(frame_.viewport);
  frame_.xyIndexStore = &xyIndexStore_;

  // Figure out our local device ID by asking the transports; the engine falls back to a random one.
  engine_.SetHasNetworks(!networks_.empty());
  NetworkDeviceId localDeviceIdFromNetworks;
  for (const Network* network : networks_) {
    NetworkDeviceId localDeviceId = network->getLocalDeviceId();
    if (localDeviceId != NetworkDeviceId()) {
      localDeviceIdFromNetworks = localDeviceId;
      break;
    }
  }
  engine_.SetupDeviceId(localDeviceIdFromNetworks);
  jll_info(
      "Starting JazzLights player %s; "
      "basePrecedence %u precedenceGain %u strands: %zu%s, "
      "pixels: %zu, %s " DEVICE_ID_FMT " w %f h %f ox %f oy %f xv %zu yv %zu",
      BOOT_MESSAGE, engine_.basePrecedence(), engine_.precedenceGain(), strands_.size(),
      strands_.empty() ? " (CONTROLLER ONLY!)" : "", frame_.pixelCount, !networks_.empty() ? "networked" : "standalone",
      DEVICE_ID_HEX(engine_.localDeviceId()), frame_.viewport.size.width, frame_.viewport.size.height,
      frame_.viewport.origin.x, frame_.viewport.origin.y, xyIndexStore_.xValuesCount(), xyIndexStore_.yValuesCount());

  ready_ = true;

#if JL_IS_CONFIG(RHINO_HAT) || JL_IS_CONFIG(RHINO_STAFF)
  static constexpr uint8_t kForestPalette = 5;
  forcePalette(kForestPalette);
#endif  // RHINO_HAT || RHINO_STAFF

#if defined(JL_START_SPECIAL) && JL_START_SPECIAL
  handleSpecial();
#elif JL_IS_CONFIG(XMAS_TREE)
  engine_.SetPatternAndLoop(kWarmPattern);
#elif JL_IS_CONFIG(HAMMER)
  // Hammer defaults to looping glow-red pattern.
  engine_.SetPatternAndLoop(0x00080000);
#elif JL_IS_CONFIG(CREATURE)
  engine_.SetPatternAndLoop(kCreaturePattern);
#elif JL_IS_CONFIG(ORRERY_PLANET)
  engine_.SetPatternAndLoop(planetPattern_);
#endif
#if defined(JL_START_LOOP) && JL_START_LOOP
  engine_.SetLooping(true);
#endif  // JL_START_LOOP
}

void Player::updatePrecedence(Precedence basePrecedence, Precedence precedenceGain) {
  if (!engine_.UpdatePrecedence(basePrecedence, precedenceGain)) { return; }
  if (!ready_) { return; }
  engine_.CheckLeaderAndPattern();
  SendPendingMessage(/*sendAsap=*/true);
}

void Player::SendPendingMessage(bool sendAsap) {
  std::optional<ProtocolMessage> messageToSend = engine_.GetMessageToSend();
  if (!messageToSend) { return; }
  for (Network* network : networks_) {
    if (!network->shouldEcho() && messageToSend->receiptNetworkId == network->id()) {
      jll_debug("Not echoing for %s to %s ", NetworkTypeToString(network->type()),
                networkMessageToString(*messageToSend).c_str());
      network->disableSending();
      continue;
    }
    jll_player_message("Setting messageToSend for %s to %s ", NetworkTypeToString(network->type()),
                       networkMessageToString(*messageToSend).c_str());
    network->setMessageToSend(*messageToSend);
  }
  if (sendAsap) {
    for (Network* network : networks_) { network->triggerSendAsap(); }
  }
}

void Player::handleSpecial() {
  static constexpr PatternBits kSpecialPatternBits[] = {
      0x00001000,  // calibration.
      0x00000000,  // black.
      0x00000100,  // red.
      0x00000200,  // green.
      0x00000300,  // blue.
      0x00000700,  // white.
  };
  specialMode_++;
  if (specialMode_ > sizeof(kSpecialPatternBits) / sizeof(kSpecialPatternBits[0])) { specialMode_ = 1; }
  engine_.SetPatternAndLoop(kSpecialPatternBits[specialMode_ - 1]);
  jll_info("Starting special mode %zu", specialMode_);
}

void Player::stopSpecial() {
  if (specialMode_ == 0) { return; }
  jll_info("Stopping special mode");
  specialMode_ = 0;
  engine_.ResumeRotation();
}

#if JL_IS_CONFIG(FAIRY_WAND)
void Player::triggerPatternOverride() {
  jll_info("Triggering pattern override");
  overridePatternStartTime_ = timeMicros();
}
#endif  // FAIRY_WAND

bool Player::render() {
  if (!ready_) { begin(); }
  const Microseconds currentTime = timeMicros();

#if JL_AUDIO_VISUALIZER
  if (sound_reactive_mode_ == SoundReactiveMode::kAuto) {
    Audio::VisualizerData data;
    Audio::Get().GetVisualizerData(&data);
    if (data.squelch) {
      if (!squelch_start_time_) {
        squelch_start_time_ = currentTime;
      } else if (!sound_reactive_suppressed_ && currentTime - *squelch_start_time_ > 30 * kMicrosecondsPerSecond) {
        sound_reactive_suppressed_ = true;
        shouldBeginPattern_ = true;
        jll_info("Auto sound reactive suppressed due to 30s squelch");
      }
    } else {
      if (sound_reactive_suppressed_) {
        sound_reactive_suppressed_ = false;
        shouldBeginPattern_ = true;
        jll_info("Auto sound reactive resumed");
      }
      squelch_start_time_.reset();
    }
  }
#endif  // JL_AUDIO_VISUALIZER

  // First listen on all networks.
  for (Network* network : networks_) {
    for (ProtocolMessage receivedMessage : network->getReceivedMessages()) {
      engine_.HandleReceivedMessage(receivedMessage);
    }
  }

  // Then react to any received packets.
  engine_.CheckLeaderAndPattern();
  SendPendingMessage();

  // Then give all networks the opportunity to send.
  for (Network* network : networks_) { network->runLoop(); }

  frame_.context = nullptr;
  const Microseconds currentPatternStartTime = engine_.currentPatternStartTime();
  if (currentTime - currentPatternStartTime > kEffectDuration) {
    frame_.pattern = engine_.GetNextPattern();
    SetFrameTime(frame_, currentTime, currentPatternStartTime + kEffectDuration);
  } else {
    frame_.pattern = engine_.GetCurrentPattern();
    SetFrameTime(frame_, currentTime, currentPatternStartTime);
  }

  if (!enabled()) {
    // TODO save CPU by not computing anything when disabled.
    frame_.pattern = 0;
  }
#if JL_IS_CONFIG(CLOUDS)
  else if (engine_.followedNextHopNetworkId() == 0) {
    if (color_overridden_) {
      frame_.pattern = color_override_.r << 24 | color_override_.g << 16 | color_override_.b << 8 | 0x20;
    } else if (force_clouds_) {
      frame_.pattern &= 0xFFFFFFF0;
      frame_.pattern |= 0x000000F0;
    }
  }
#endif  // CLOUDS

#if JL_IS_CONFIG(ORRERY_PLANET)
  if (!engine_.creatureIsFollowingNonCreature()) { frame_.pattern = planetPattern_; }
#endif  // ORRERY_PLANET

  const Effect* effect = patternFromBits(frame_.pattern, *this);
#if JL_IS_CONFIG(FAIRY_WAND)
  constexpr Microseconds kOverridePatternDuration = 8 * kMicrosecondsPerSecond;  // 8s.
  static const FunctionalEffect fairy_wand_effect = fairy_wand();
  if (overridePatternStartTime_) {
    if (currentTime - *overridePatternStartTime_ < kOverridePatternDuration) {
      SetFrameTime(frame_, currentTime, *overridePatternStartTime_);
      effect = &fairy_wand_effect;
    }
  }
#elif JL_IS_CONFIG(CREATURE)
  if (!engine_.creatureIsFollowingNonCreature()) { effect = patternFromBits(kCreaturePattern, *this); }
#elif JL_IS_CONFIG(ORRERY_PLANET)
  if (!engine_.creatureIsFollowingNonCreature()) { effect = patternFromBits(planetPattern_, *this); }
#endif  // FAIRY_WAND

  // Ensure effectContext_ is big enough for this effect.
  size_t effectContextSize = effect->contextSize(frame_);
  if (effectContextSize > effectContextSize_) {
    if ((effectContextSize % kMaxStateAlignment) != 0) {
      // aligned_alloc required the allocation size to be a multiple of the alignment.
      effectContextSize += kMaxStateAlignment - (effectContextSize % kMaxStateAlignment);
    }
    jll_info("realloc context size from %zu to %zu (%s w %f h %f xv %zu yv %zu)", effectContextSize_, effectContextSize,
             effect->effectName(frame_.pattern).c_str(), frame_.viewport.size.width, frame_.viewport.size.height,
             xyIndexStore_.xValuesCount(), xyIndexStore_.yValuesCount());
    // realloc doesn't support alignment requirements, so we need to use aligned_alloc and copy the data ourselves.
    size_t previousContextSize = effectContextSize_;
    void* previousContext = effectContext_;
    effectContextSize_ = effectContextSize;
    effectContext_ = aligned_alloc(kMaxStateAlignment, effectContextSize_);
    if (effectContext_ == nullptr) {
      jll_fatal("aligned_alloc(%zu, %zu) failed", kMaxStateAlignment, effectContextSize_);
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
    lastLEDWriteTime_.reset();
  }

  // Do not send data to LEDs faster than 100Hz.
  if (lastLEDWriteTime_ && *lastLEDWriteTime_ <= currentTime) {
    static constexpr Microseconds kMinLEDWriteTime = 10 * kMicrosecondsPerMillisecond;
    Microseconds timeSinceLastWrite = currentTime - *lastLEDWriteTime_;
    if (timeSinceLastWrite < kMinLEDWriteTime) {
#if JL_PLAYER_SLEEPS
      // Note that this mode ends up operating at 90Hz since one spin of the runloop isn't free.
      // We could tweak it if we cared more about FPS than battery life.
      vTaskDelay((kMinLEDWriteTime - timeSinceLastWrite) / kMicrosecondsPerMillisecond / portTICK_PERIOD_MS);
#endif  // JL_PLAYER_SLEEPS
      return false;
    }
  }
  lastLEDWriteTime_ = currentTime;

#if JL_IS_CONFIG(CREATURE)
  KnownCreatures::Get()->ExpireOldEntries();
#endif  // CREATURE

  const Microseconds patternComputeStartTime = timeMicros();
  // Actually render the pixels.
  predictableRandom_.ResetWithFrameTime(frame_, effect->effectName(frame_.pattern).c_str());
  effect->rewind(frame_);
  Pixel px;
  size_t cumulativeIndex = 0;
  for (const Strand& s : strands_) {
    px.strand = &s;
    const size_t numPixels = s.layout.pixelCount();
    for (size_t index = 0; index < numPixels; index++) {
      CRGB color;
      px.coord = s.layout.at(index);
      if (!IsEmpty(px.coord)) {
        px.strandIndex = index;
        px.cumulativeIndex = cumulativeIndex;
        color = effect->color(frame_, px);
      } else {
        color = CRGB::Black;
      }
      cumulativeIndex++;
      s.renderer.renderPixel(index, color);
    }
  }
  effect->afterColors(frame_);

  // Save data for measuring FPS.
  const Microseconds patternComputeDuration = timeMicros() - patternComputeStartTime;
  timeSpentComputingEffectsThisEpoch_ += patternComputeDuration;
  framesComputedThisEpoch_++;

  return true;
}

void Player::GenerateFPSReport(uint16_t* fpsCompute, uint16_t* fpsWrites, uint8_t* utilization,
                               Microseconds* timeSpentComputingThisEpoch, Microseconds* epochDuration) {
  const Microseconds currentTime = timeMicros();
  *epochDuration = currentTime - fpsEpochStart_;
  fpsEpochStart_ = currentTime;
  *timeSpentComputingThisEpoch = timeSpentComputingEffectsThisEpoch_;
  timeSpentComputingEffectsThisEpoch_ = 0;
  uint32_t numLedWritesThisEpoch = 0;
  if (numLedWritesGetter_ != nullptr) { numLedWritesThisEpoch = numLedWritesGetter_->GetAndClearNumWrites(); }
  if (*epochDuration != 0) {
    *fpsCompute = framesComputedThisEpoch_ * kMicrosecondsPerSecond / *epochDuration;
    *fpsWrites = numLedWritesThisEpoch * kMicrosecondsPerSecond / *epochDuration;
    *utilization = *timeSpentComputingThisEpoch * 100 / *epochDuration;
  } else {
    *fpsCompute = 0;
    *fpsWrites = 0;
    *utilization = 0;
  }
  framesComputedThisEpoch_ = 0;
}

PatternBits Player::currentEffect() const { return lastBegunPattern_; }
std::string Player::currentEffectName() const { return patternName(lastBegunPattern_, *this); }

void Player::set_enabled(bool enabled) {
  if (enabled_ == enabled) { return; }
#if JL_IS_CONFIG(CLOUDS)
  engine_.ClearUserInputTime();
  if (!enabled) {
    force_clouds_ = true;
    engine_.ReapplyForcedPalette();
  }
#endif  // CLOUDS
  enabled_ = enabled;
  UpdateStatusWatcher();
}

#if JL_AUDIO_VISUALIZER
bool Player::sound_reactive_enabled() const {
  if (sound_reactive_mode_ == SoundReactiveMode::kOff) { return false; }
  if (sound_reactive_mode_ == SoundReactiveMode::kOn) { return true; }
  return !sound_reactive_suppressed_;
}

void Player::set_sound_reactive_mode(SoundReactiveMode mode) {
  if (sound_reactive_mode_ == mode) { return; }
  sound_reactive_mode_ = mode;
  sound_reactive_suppressed_ = false;
  squelch_start_time_.reset();
  shouldBeginPattern_ = true;
}
#endif  // JL_AUDIO_VISUALIZER

void Player::set_brightness(uint8_t brightness) {
  if (brightness_ == brightness) { return; }
  brightness_ = brightness;
  UpdateStatusWatcher();
}

void Player::UpdateStatusWatcher() {
#if JL_IS_CONFIG(CLOUDS)
  if (status_watcher_ != nullptr) { status_watcher_->OnStatus(); }
#endif  // CLOUDS
}

#if JL_IS_CONFIG(CLOUDS)
void Player::CloudNext() {
  set_enabled(true);
  disable_color_override();
  const bool extraAdvance = force_clouds_;
  force_clouds_ = false;
  engine_.CloudNext(extraAdvance);
  SendPendingMessage(/*sendAsap=*/true);
  if (status_watcher_ != nullptr) { status_watcher_->OnStatus(); }
}
#endif  // CLOUDS

void Player::next() {
#if JL_IS_CONFIG(CLOUDS)
  set_enabled(!enabled());
#endif  // CLOUDS
  engine_.GoToNextPattern();
  SendPendingMessage(/*sendAsap=*/true);
}

void Player::setPattern(PatternBits pattern) {
  engine_.SetPattern(pattern);
  SendPendingMessage(/*sendAsap=*/true);
}

void Player::forcePalette(uint8_t palette) {
  engine_.ForcePalette(palette);
  SendPendingMessage(/*sendAsap=*/true);
}

std::string Player::PatternName(PatternBits pattern) const { return patternName(pattern, *this); }

std::optional<PatternBits> Player::ForcedLeadingPattern() const {
#if JL_IS_CONFIG(XMAS_TREE)
  return kWarmPattern;
#elif JL_IS_CONFIG(CREATURE)
  return kCreaturePattern;
#elif JL_IS_CONFIG(ORRERY_PLANET)
  return planetPattern_;
#else
  return std::nullopt;
#endif
}

void Player::OnPatternRestart() {
  lastLEDWriteTime_.reset();
  shouldBeginPattern_ = true;
}

void Player::OnAcceptedUpdate() { lastLEDWriteTime_.reset(); }

void Player::LogFpsReport() {
  uint16_t fpsCompute;
  uint16_t fpsWrites;
  uint8_t utilization;
  Microseconds timeSpentComputingThisEpoch;
  Microseconds epochDuration;
  GenerateFPSReport(&fpsCompute, &fpsWrites, &utilization, &timeSpentComputingThisEpoch, &epochDuration);
  jll_protocol_info("computed %u FPS wrote %u FPS %u%% %lld/%lldms", fpsCompute, fpsWrites, utilization,
                    MsForLogs(timeSpentComputingThisEpoch), MsForLogs(epochDuration));
  printInstrumentationInfo();
}

#if JL_IS_CONFIG(CREATURE)
bool Player::IsPartying() const { return KnownCreatures::Get()->IsPartying(); }

uint32_t Player::CreatureColor() const { return ThisCreatureColor(); }

void Player::OnCreatureHeard(uint32_t creatureColor, Microseconds heardTime, int rssi, bool isPartying) {
  KnownCreatures::Get()->AddCreature(creatureColor, heardTime, rssi, isPartying);
}

void Player::OnOrreryHeard() { KnownCreatures::Get()->HandleHeardOrrery(); }
#endif  // CREATURE

const char* Player::command(const char* req) {
  static char res[256];
  const size_t MAX_CMD_LEN = 16;
  bool responded = false;

  if (!strncmp(req, "status?", MAX_CMD_LEN)) {
    // do nothing
  } else if (!strncmp(req, "next", MAX_CMD_LEN)) {
    stopLooping();
    next();
  } else if (!strncmp(req, "prev", MAX_CMD_LEN)) {
    loopOne();
  } else {
    snprintf(res, sizeof(res), "! unknown command");
    responded = true;
  }
  if (!responded) {
    // This is used by the WebUI to display the current pattern name.
    snprintf(res, sizeof(res), "playing %s", patternName(lastBegunPattern_, *this).c_str());
  }
  jll_debug("[%s] -> [%s]", req, res);
  return res;
}

}  // namespace jazzlights
