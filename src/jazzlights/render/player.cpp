#include "jazzlights/render/player.h"

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
#include "jazzlights/network/manager.h"
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

namespace jazzlights {
namespace {
#if JL_IS_CONFIG(XMAS_TREE)
constexpr PatternBits kWarmPattern = 0x00001500;
#elif JL_IS_CONFIG(CREATURE)
constexpr PatternBits kCreaturePattern = 0x0000FF00;
#endif
}  // namespace

static constexpr CRGB WarmColor() {
  // Based on the example values from:
  // https://www.usuallypragmatic.com/projects/Generating-Color-Temperature-Equivalent-Light-with-RGB-LEDs.html
  return CRGB(255, 67, 5);
}

static const Effect* PatternFromBits(PatternBits pattern, const Player& player) {
  // Static definitions of all patterns.
  static const SpinPlasma kSpinPattern;
  static const Hiphotic kHiphoticPattern;
  static const Metaballs kMetaballsPattern;
  static const ColoredBursts kColoredBurstsPattern;
  static const Flame kFlamePattern;
  static const Glitter kGlitterPattern;
  static const TheMatrix kTheMatrixPattern;
  static const Rings kRingsPattern;
#if JL_AUDIO_VISUALIZER
  static const SoundEffect kSoundEffect;
#endif  // JL_AUDIO_VISUALIZER
  static const FunctionalEffect kThreesinePattern = Threesine();
  static const FunctionalEffect kFollowStrandEffect = FollowStrand();
  static const FunctionalEffect kMappingEffect = Mapping();
  static const FunctionalEffect kColoringEffect = Coloring();
  static const FunctionalEffect kCalibrationEffect = Calibration();
  static const FunctionalEffect kSyncTestEffect = SyncTest();
  static const FunctionalEffect kBlackEffect = Solid(CRGB::Black, "black");
  static const FunctionalEffect kRedEffect = Solid(CRGB::Red, "red");
  static const FunctionalEffect kGreenEffect = Solid(CRGB::Green, "green");
  static const FunctionalEffect kBlueEffect = Solid(CRGB::Blue, "blue");
  static const FunctionalEffect kPurpleEffect = Solid(CRGB::Purple, "purple");
  static const FunctionalEffect kCyanEffect = Solid(CRGB::Cyan, "cyan");
  static const FunctionalEffect kYellowEffect = Solid(CRGB::Yellow, "yellow");
  static const FunctionalEffect kWhiteEffect = Solid(CRGB::White, "white");
  static const FunctionalEffect kWarmEffect = Solid(WarmColor(), "warm");
  static const FunctionalEffect kRedGlowEffect = Glow(CRGB::Red, "glow-red");
  static const FunctionalEffect kGreenGlowEffect = Glow(CRGB::Green, "glow-green");
  static const FunctionalEffect kBlueGlowEffect = Glow(CRGB::Blue, "glow-blue");
  static const FunctionalEffect kPurpleGlowEffect = Glow(CRGB::Purple, "glow-purple");
  static const FunctionalEffect kCyanGlowEffect = Glow(CRGB::Cyan, "glow-cyan");
  static const FunctionalEffect kYellowGlowEffect = Glow(CRGB::Yellow, "glow-yellow");
  static const FunctionalEffect kWhiteGlowEffect = Glow(CRGB::White, "glow-white");
  static const FunctionalEffect kWarmGlowEffect = Glow(WarmColor(), "glow-warm");
#if JL_IS_CONFIG(CLOUDS)
  static const Clouds kCloudsEffect = Clouds();
#elif JL_IS_CONFIG(CREATURE)
  static const Creatures kCreaturesEffect = Creatures();
#endif

  // Pattern selection from bits.
  // If the pattern bits have the four least-significant bits all zero then this is a reserved pattern,
  // and we examine the next four bits to determine what *type* of reserved pattern it is.
  // If the next four bits are also zero (reserved type zero),
  // then we examine the next eight bits to determine which particular one
  // of the reserved type zero patterns it is (generally simple solid colors).
  // Reserved types 0 (basic), 1 (mapping), and 2 (coloring) don’t use a palette.
  // Reserved type 3 and the non-reserved patterns do use ColorWithPalette.
  if (PatternIsReserved(pattern)) {
    const uint8_t reservedType = (pattern >> 4) & 0xF;
    if (reservedType == 0x0) {
      switch ((pattern >> 8) & 0xFF) {
        case 0x00: return &kBlackEffect;
        case 0x01: return &kRedEffect;
        case 0x02: return &kGreenEffect;
        case 0x03: return &kBlueEffect;
        case 0x04: return &kPurpleEffect;
        case 0x05: return &kCyanEffect;
        case 0x06: return &kYellowEffect;
        case 0x07: return &kWhiteEffect;
        case 0x08: return &kRedGlowEffect;
        case 0x09: return &kGreenGlowEffect;
        case 0x0A: return &kBlueGlowEffect;
        case 0x0B: return &kPurpleGlowEffect;
        case 0x0C: return &kCyanGlowEffect;
        case 0x0D: return &kYellowGlowEffect;
        case 0x0E: return &kWhiteGlowEffect;
        case 0x0F: return &kSyncTestEffect;
        case 0x10: return &kCalibrationEffect;
        case 0x11: return &kFollowStrandEffect;
        case 0x12: return &kGlitterPattern;
        case 0x13: return &kTheMatrixPattern;
        case 0x14: return &kThreesinePattern;
        case 0x15: return &kWarmEffect;
        case 0x16: return &kWarmGlowEffect;
#if JL_IS_CONFIG(ORRERY_PLANET)
        case 0xFE: return PlanetEffect::Get();
#endif  // ORRERY_PLANET
        case 0xFF:
#if JL_IS_CONFIG(CREATURE)
          return &kCreaturesEffect;
#else   // CREATURE
          return &kWhiteGlowEffect;
#endif  // CREATURE
      }
    } else if (reservedType == 0x1) {
      return &kMappingEffect;
    } else if (reservedType == 0x2) {
      return &kColoringEffect;
    } else if (reservedType == 0x3) {
      // Reserved effects that use a palette.
      switch ((pattern >> 8) & 0xF) {
        case 0x0:  // Use the pattern bits.
          if (PatternBit(pattern, 1)) {
            return &kMetaballsPattern;
          } else {
            return &kColoredBurstsPattern;
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
    else if (reservedType == 0xF) {
      return &kCloudsEffect;
    }
#endif  // CLOUDS
    return &kRedEffect;
  } else {
#if JL_AUDIO_VISUALIZER
    if (player.SoundReactiveEnabled()) { return &kSoundEffect; }
#else   // JL_AUDIO_VISUALIZER
    (void)player;
#endif  // JL_AUDIO_VISUALIZER
    if (PatternBit(pattern, 1)) {
      if (PatternBit(pattern, 2) && !player.isAllLinear()) {  // 11x - spin
        return &kSpinPattern;
      } else {  // 10x - hiphotic
        return &kHiphoticPattern;
      }
    } else {
#if JL_PLAYER_SKIP_FLAME
      return &kRingsPattern;
#else   // JL_PLAYER_SKIP_FLAME
      if (PatternBit(pattern, 2) && !player.isAllLinear()) {  // 01x - flame
        return &kFlamePattern;
      } else {  // 00x - rings
        return &kRingsPattern;
      }
#endif  // JL_PLAYER_SKIP_FLAME
    }
  }
  jll_fatal("Failed to pick an effect %s", DisplayBitsAsBinary(pattern).c_str());
}

std::string PatternName(PatternBits pattern, const Player& player) {
  return PatternFromBits(pattern, player)->EffectName(pattern);
}

Player::Player(NetworkManager& networkManager) : networkManager_(networkManager) {
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

Player& Player::AddStrand(const Layout& l, Renderer& r) {
  strands_.push_back({l, r, strands_.size()});
  return *this;
}

void Player::Begin() {
  xyIndexStore_.Reset();
  frame_.pixelCount = 0;
  frame_.viewport.origin.x = 0;
  frame_.viewport.origin.y = 0;
  frame_.viewport.size.height = 0;
  frame_.viewport.size.width = 0;
  for (const Strand& s : strands_) {
    frame_.viewport = Merge(frame_.viewport, jazzlights::Bounds(s.layout));
    frame_.pixelCount += s.layout.PixelCount();
    xyIndexStore_.IngestLayout(&s.layout);
  }
  if (frame_.viewport.size.width == 0 || frame_.viewport.size.height == 0) { isAllLinear_ = true; }
  xyIndexStore_.Finalize(frame_.viewport);
  frame_.xyIndexStore = &xyIndexStore_;

  // Figure out our local device ID by asking the transports; the engine falls back to a random one.
  engine_.SetHasNetworks(networkManager_.HasNetworks());
  engine_.SetupDeviceId(networkManager_.GetLocalDeviceId());
  jll_info(
      "Starting JazzLights player %s; "
      "basePrecedence %u precedenceGain %u strands: %zu%s, "
      "pixels: %zu, %s " DEVICE_ID_FMT " w %f h %f ox %f oy %f xv %zu yv %zu",
      BOOT_MESSAGE, engine_.basePrecedence(), engine_.precedenceGain(), strands_.size(),
      strands_.empty() ? " (CONTROLLER ONLY!)" : "", frame_.pixelCount,
      networkManager_.HasNetworks() ? "networked" : "standalone", DEVICE_ID_HEX(engine_.localDeviceId()),
      frame_.viewport.size.width, frame_.viewport.size.height, frame_.viewport.origin.x, frame_.viewport.origin.y,
      xyIndexStore_.xValuesCount(), xyIndexStore_.yValuesCount());

  ready_ = true;

#if JL_IS_CONFIG(RHINO_HAT) || JL_IS_CONFIG(RHINO_STAFF)
  static constexpr uint8_t kForestPalette = 5;
  ForcePalette(kForestPalette);
#endif  // RHINO_HAT || RHINO_STAFF

#if defined(JL_START_SPECIAL) && JL_START_SPECIAL
  HandleSpecial();
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

void Player::UpdatePrecedence(Precedence basePrecedence, Precedence precedenceGain) {
  if (!engine_.UpdatePrecedence(basePrecedence, precedenceGain)) { return; }
  if (!ready_) { return; }
  engine_.CheckLeaderAndPattern();
  SendPendingMessage(/*sendAsap=*/true);
}

void Player::SendPendingMessage(bool sendAsap) {
  networkManager_.SetMessageToSend(engine_.GetMessageToSend(), sendAsap);
}

void Player::HandleSpecial() {
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

void Player::StopSpecial() {
  if (specialMode_ == 0) { return; }
  jll_info("Stopping special mode");
  specialMode_ = 0;
  engine_.ResumeRotation();
}

#if JL_IS_CONFIG(FAIRY_WAND)
void Player::TriggerPatternOverride() {
  jll_info("Triggering pattern override");
  overridePatternStartTime_ = TimeMicros();
}
#endif  // FAIRY_WAND

bool Player::Render() {
  if (!ready_) { Begin(); }
  const Microseconds currentTime = TimeMicros();

#if JL_AUDIO_VISUALIZER
  if (soundReactiveMode_ == SoundReactiveMode::kAuto) {
    Audio::VisualizerData data;
    Audio::Get().GetVisualizerData(&data);
    if (data.squelch) {
      if (!squelchStartTime_) {
        squelchStartTime_ = currentTime;
      } else if (!soundReactiveSuppressed_ && currentTime - *squelchStartTime_ > 30 * kMicrosecondsPerSecond) {
        soundReactiveSuppressed_ = true;
        shouldBeginPattern_ = true;
        jll_info("Auto sound reactive suppressed due to 30s squelch");
      }
    } else {
      if (soundReactiveSuppressed_) {
        soundReactiveSuppressed_ = false;
        shouldBeginPattern_ = true;
        jll_info("Auto sound reactive resumed");
      }
      squelchStartTime_.reset();
    }
  }
#endif  // JL_AUDIO_VISUALIZER

  // First listen on all networks.
  for (const ProtocolMessage& receivedMessage : networkManager_.GetReceivedMessages()) {
    engine_.HandleReceivedMessage(receivedMessage);
  }

  // Then react to any received packets.
  engine_.CheckLeaderAndPattern();
  SendPendingMessage();

  // Then give all networks the opportunity to send.
  networkManager_.RunLoop();

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
    if (colorOverridden_) {
      frame_.pattern = colorOverride_.r << 24 | colorOverride_.g << 16 | colorOverride_.b << 8 | 0x20;
    } else if (forceClouds_) {
      frame_.pattern &= 0xFFFFFFF0;
      frame_.pattern |= 0x000000F0;
    }
  }
#endif  // CLOUDS

#if JL_IS_CONFIG(ORRERY_PLANET)
  if (!engine_.creatureIsFollowingNonCreature()) { frame_.pattern = planetPattern_; }
#endif  // ORRERY_PLANET

  const Effect* effect = PatternFromBits(frame_.pattern, *this);
#if JL_IS_CONFIG(FAIRY_WAND)
  constexpr Microseconds kOverridePatternDuration = 8 * kMicrosecondsPerSecond;  // 8s.
  static const FunctionalEffect kFairyWandEffect = FairyWand();
  if (overridePatternStartTime_) {
    if (currentTime - *overridePatternStartTime_ < kOverridePatternDuration) {
      SetFrameTime(frame_, currentTime, *overridePatternStartTime_);
      effect = &kFairyWandEffect;
    }
  }
#elif JL_IS_CONFIG(CREATURE)
  if (!engine_.creatureIsFollowingNonCreature()) { effect = PatternFromBits(kCreaturePattern, *this); }
#elif JL_IS_CONFIG(ORRERY_PLANET)
  if (!engine_.creatureIsFollowingNonCreature()) { effect = PatternFromBits(planetPattern_, *this); }
#endif  // FAIRY_WAND

  // Ensure effectContext_ is big enough for this effect.
  size_t effectContextSize = effect->ContextSize(frame_);
  if (effectContextSize > effectContextSize_) {
    if ((effectContextSize % kMaxStateAlignment) != 0) {
      // aligned_alloc required the allocation size to be a multiple of the alignment.
      effectContextSize += kMaxStateAlignment - (effectContextSize % kMaxStateAlignment);
    }
    jll_info("realloc context size from %zu to %zu (%s w %f h %f xv %zu yv %zu)", effectContextSize_, effectContextSize,
             effect->EffectName(frame_.pattern).c_str(), frame_.viewport.size.width, frame_.viewport.size.height,
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
    predictableRandom_.ResetWithFrameStart(frame_, effect->EffectName(frame_.pattern).c_str());
    effect->Begin(frame_);
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

  const Microseconds patternComputeStartTime = TimeMicros();
  // Actually render the pixels.
  predictableRandom_.ResetWithFrameTime(frame_, effect->EffectName(frame_.pattern).c_str());
  effect->Rewind(frame_);
  Pixel px;
  size_t cumulativeIndex = 0;
  for (const Strand& s : strands_) {
    px.strand = &s;
    const size_t numPixels = s.layout.PixelCount();
    for (size_t index = 0; index < numPixels; index++) {
      CRGB color;
      px.coord = s.layout.At(index);
      if (!IsEmpty(px.coord)) {
        px.strandIndex = index;
        px.cumulativeIndex = cumulativeIndex;
        color = effect->Color(frame_, px);
      } else {
        color = CRGB::Black;
      }
      cumulativeIndex++;
      s.renderer.RenderPixel(index, color);
    }
  }
  effect->AfterColors(frame_);

  // Save data for measuring FPS.
  const Microseconds patternComputeDuration = TimeMicros() - patternComputeStartTime;
  timeSpentComputingEffectsThisEpoch_ += patternComputeDuration;
  framesComputedThisEpoch_++;

  return true;
}

void Player::GenerateFPSReport(uint16_t* fpsCompute, uint16_t* fpsWrites, uint8_t* utilization,
                               Microseconds* timeSpentComputingThisEpoch, Microseconds* epochDuration) {
  const Microseconds currentTime = TimeMicros();
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

PatternBits Player::CurrentEffect() const { return lastBegunPattern_; }
std::string Player::CurrentEffectName() const { return jazzlights::PatternName(lastBegunPattern_, *this); }

void Player::SetEnabled(bool enabled) {
  if (enabled_ == enabled) { return; }
#if JL_IS_CONFIG(CLOUDS)
  engine_.ClearUserInputTime();
  if (!enabled) {
    forceClouds_ = true;
    engine_.ReapplyForcedPalette();
  }
#endif  // CLOUDS
  enabled_ = enabled;
  UpdateStatusWatcher();
}

#if JL_AUDIO_VISUALIZER
bool Player::SoundReactiveEnabled() const {
  if (soundReactiveMode_ == SoundReactiveMode::kOff) { return false; }
  if (soundReactiveMode_ == SoundReactiveMode::kOn) { return true; }
  return !soundReactiveSuppressed_;
}

void Player::SetSoundReactiveMode(SoundReactiveMode mode) {
  if (soundReactiveMode_ == mode) { return; }
  soundReactiveMode_ = mode;
  soundReactiveSuppressed_ = false;
  squelchStartTime_.reset();
  shouldBeginPattern_ = true;
}
#endif  // JL_AUDIO_VISUALIZER

void Player::SetBrightness(uint8_t brightness) {
  if (brightness_ == brightness) { return; }
  brightness_ = brightness;
  UpdateStatusWatcher();
}

void Player::UpdateStatusWatcher() {
#if JL_IS_CONFIG(CLOUDS)
  if (statusWatcher_ != nullptr) { statusWatcher_->OnStatus(); }
#endif  // CLOUDS
}

#if JL_IS_CONFIG(CLOUDS)
void Player::CloudNext() {
  SetEnabled(true);
  DisableColorOverride();
  const bool extraAdvance = forceClouds_;
  forceClouds_ = false;
  engine_.CloudNext(extraAdvance);
  SendPendingMessage(/*sendAsap=*/true);
  if (statusWatcher_ != nullptr) { statusWatcher_->OnStatus(); }
}
#endif  // CLOUDS

void Player::Next() {
#if JL_IS_CONFIG(CLOUDS)
  SetEnabled(!enabled());
#endif  // CLOUDS
  engine_.GoToNextPattern();
  SendPendingMessage(/*sendAsap=*/true);
}

void Player::SetPattern(PatternBits pattern) {
  engine_.SetPattern(pattern);
  SendPendingMessage(/*sendAsap=*/true);
}

void Player::ForcePalette(uint8_t palette) {
  engine_.ForcePalette(palette);
  SendPendingMessage(/*sendAsap=*/true);
}

std::string Player::PatternName(PatternBits pattern) const { return jazzlights::PatternName(pattern, *this); }

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
  jll_protocol_info("Computed %u FPS wrote %u FPS %u%% %lld/%lldms", fpsCompute, fpsWrites, utilization,
                    MsForLogs(timeSpentComputingThisEpoch), MsForLogs(epochDuration));
  PrintInstrumentationInfo();
}

#if JL_IS_CONFIG(CREATURE)
bool Player::IsPartying() const { return KnownCreatures::Get()->IsPartying(); }

uint32_t Player::CreatureColor() const { return ThisCreatureColor(); }

void Player::OnCreatureHeard(uint32_t creatureColor, Microseconds heardTime, int rssi, bool isPartying) {
  KnownCreatures::Get()->AddCreature(creatureColor, heardTime, rssi, isPartying);
}

void Player::OnOrreryHeard() { KnownCreatures::Get()->HandleHeardOrrery(); }
#endif  // CREATURE

const char* Player::Command(const char* req) {
  static char res[256];
  const size_t MAX_CMD_LEN = 16;
  bool responded = false;

  if (!strncmp(req, "status?", MAX_CMD_LEN)) {
    // do nothing
  } else if (!strncmp(req, "next", MAX_CMD_LEN)) {
    StopLooping();
    Next();
  } else if (!strncmp(req, "prev", MAX_CMD_LEN)) {
    LoopOne();
  } else {
    snprintf(res, sizeof(res), "! unknown command");
    responded = true;
  }
  if (!responded) {
    // This is used by the WebUI to display the current pattern name.
    snprintf(res, sizeof(res), "playing %s", jazzlights::PatternName(lastBegunPattern_, *this).c_str());
  }
  jll_debug("[%s] -> [%s]", req, res);
  return res;
}

}  // namespace jazzlights
