#ifndef JL_RENDER_PLAYER_H
#define JL_RENDER_PLAYER_H

#include <optional>
#include <vector>

#include "jazzlights/effect/effect.h"
#include "jazzlights/layout/layout.h"
#include "jazzlights/network/manager.h"
#include "jazzlights/protocol/engine.h"
#include "jazzlights/render/predictable_random.h"
#include "jazzlights/render/renderer.h"
#include "jazzlights/render/xy_index.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

// Player renders patterns onto strands. The mechanics of the synchronization protocol - leader election, the
// originator table, and which pattern everyone agrees to display - live in ProtocolEngine, which Player owns and
// feeds with the messages it gets from the NetworkManager. The transports themselves belong to that NetworkManager,
// which Player does not own and which must outlive it.
class Player : private ProtocolEngine::Delegate {
 public:
  explicit Player(NetworkManager& networkManager);
  ~Player();

  // Disallow copy, allow move
  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

  // Constructing the player
  Player& AddStrand(const Layout& l, Renderer& r);

  /**
   * Prepare for rendering
   *
   * Call this when you're done adding strands, setting up
   * player configuration and connecting networks.
   */
  void Begin();

  /**
   *  Render current frame to all strands.
   *  Returns whether the caller should send data to the LEDs.
   */
  bool Render();

  /**
   *  Play next effect in the playlist
   */
  void Next();

  /**
   *  Loop current effect forever.
   */
  void LoopOne() { engine_.LoopOne(); }

  /**
   *  Stop looping current effect forever.
   */
  void StopLooping() { engine_.StopLooping(); }

  /**
   *  Returns whether we are looping current effect forever.
   */
  bool isLooping() const { return engine_.isLooping(); }

  /**
   *  Sets the current pattern and correspondingly resets the next pattern.
   */
  void SetPattern(PatternBits pattern);

  /**
   *  Forces the rotation to always use this palette.
   */
  void ForcePalette(uint8_t palette);

  /**
   *  Cancels previous call to ForcePalette.
   */
  void StopForcePalette() { engine_.StopForcePalette(); }

  /**
   * Returns the currently forced palette.
   */
  std::optional<uint8_t> forcedPalette() const { return engine_.forcedPalette(); }

  /**
   * Run text command
   */
  const char* Command(const char* cmd);

  /**
   * Computes FPS information and resets counters.
   */
  void GenerateFPSReport(uint16_t* fpsCompute, uint16_t* fpsWrites, uint8_t* utilization,
                         Microseconds* timeSpentComputingThisEpoch, Microseconds* epochDuration);

  /**
   * Returns the bounding box of all pixels
   */
  const Box& bounds() const { return frame_.viewport; }

  void HandleSpecial();
  void StopSpecial();
  size_t GetSpecial() const { return specialMode_; }
#if JL_IS_CONFIG(FAIRY_WAND)
  void TriggerPatternOverride();
#endif  // FAIRY_WAND

  bool isPowerLimited() const { return powerLimited_; }
  void SetPowerLimited(bool powerLimited) { powerLimited_ = powerLimited; }
  uint8_t brightness() const { return brightness_; }
  void SetBrightness(uint8_t brightness);

  void SetBasePrecedence(Precedence basePrecedence) { engine_.SetBasePrecedence(basePrecedence); }
  void SetPrecedenceGain(Precedence precedenceGain) { engine_.SetPrecedenceGain(precedenceGain); }
  void UpdatePrecedence(Precedence basePrecedence, Precedence precedenceGain);
  void SetRandomizeLocalDeviceId(bool val) { engine_.SetRandomizeLocalDeviceId(val); }

  PredictableRandom* predictableRandom() { return &predictableRandom_; }
  PatternBits CurrentEffect() const;
  std::string CurrentEffectName() const;
  NetworkType following() const { return engine_.following(); }
  NumHops currentNumHops() const { return engine_.currentNumHops(); }

  bool enabled() const { return enabled_; }
  void SetEnabled(bool enabled);

#if JL_AUDIO_VISUALIZER
  // Work around clang-format disagreement between CI and local copy.
  // clang-format off
  enum class SoundReactiveMode {
    kOff,
    kOn,
    kAuto,
  };
  // clang-format on
  SoundReactiveMode soundReactiveMode() const { return soundReactiveMode_; }
  void SetSoundReactiveMode(SoundReactiveMode mode);
  bool SoundReactiveEnabled() const;
#endif  // JL_AUDIO_VISUALIZER

#if JL_IS_CONFIG(CLOUDS)
  class StatusWatcher {
   public:
    virtual ~StatusWatcher() = default;
    virtual void OnStatus() = 0;
  };
  void SetStatusWatcher(StatusWatcher* statusWatcher) { statusWatcher_ = statusWatcher; }
  void EnableColorOverride(CRGB colorOverride) { colorOverride_ = colorOverride; }
  void DisableColorOverride() { colorOverride_.reset(); }
  const std::optional<CRGB>& colorOverride() const { return colorOverride_; }
  void CloudNext();
#elif JL_IS_CONFIG(ORRERY_PLANET)
  void SetPlanetPattern(PatternBits planetPattern) { planetPattern_ = planetPattern; }
  PatternBits GetPlanetPattern() const { return planetPattern_; }
#elif JL_IS_CONFIG(ORRERY_LEADER)
  using OverriddenPatternWatcher = ProtocolEngine::OverriddenPatternWatcher;
  void SetOverriddenPatternWatcher(OverriddenPatternWatcher* overriddenPatternWatcher) {
    engine_.SetOverriddenPatternWatcher(overriddenPatternWatcher);
  }
#endif
  using OrrerySceneIdWatcher = ProtocolEngine::OrrerySceneIdWatcher;
  void SetOrrerySceneIdWatcher(OrrerySceneIdWatcher* orrerySceneIdWatcher) {
    engine_.SetOrrerySceneIdWatcher(orrerySceneIdWatcher);
  }

  class NumLedWritesGetter {
   public:
    virtual ~NumLedWritesGetter() = default;
    virtual uint32_t GetAndClearNumWrites() = 0;
  };
  void SetNumLedWritesGetter(NumLedWritesGetter* numLedWritesGetter) { numLedWritesGetter_ = numLedWritesGetter; }

  void SetOrrerySceneIdToSend(std::optional<OrrerySceneId> orrerySceneIdToSend) {
    engine_.SetOrrerySceneIdToSend(orrerySceneIdToSend);
  }

  bool isAllLinear() const { return isAllLinear_; }

 private:
  void UpdateStatusWatcher();

  // From ProtocolEngine::Delegate.
  std::string PatternName(PatternBits pattern) const override;
  std::optional<PatternBits> ForcedLeadingPattern() const override;
  void OnPatternRestart() override;
  void OnAcceptedUpdate() override;
  void LogFpsReport() override;
#if JL_IS_CONFIG(CREATURE)
  bool IsPartying() const override;
  uint32_t CreatureColor() const override;
  void OnCreatureHeard(uint32_t creatureColor, Microseconds heardTime, int rssi, bool isPartying) override;
  void OnOrreryHeard() override;
#endif  // CREATURE

  void SendPendingMessage(bool sendAsap = false);

#if JL_IS_CONFIG(CLOUDS) && !JL_DEV
  bool enabled_ = false;
#else   // JL_IS_CONFIG(CLOUDS) && !JL_DEV
  bool enabled_ = true;
#endif  // JL_IS_CONFIG(CLOUDS) && !JL_DEV

#if JL_AUDIO_VISUALIZER
  SoundReactiveMode soundReactiveMode_ = SoundReactiveMode::kAuto;
  bool soundReactiveSuppressed_ = false;
  OptionalMicroseconds squelchStartTime_;
#endif  // JL_AUDIO_VISUALIZER

  bool ready_ = false;
  bool powerLimited_ = false;
  uint8_t brightness_ = 255;

  std::vector<Strand> strands_;

  void* effectContext_ = nullptr;
  size_t effectContextSize_ = 0;

  PatternBits lastBegunPattern_ = 0;
  bool shouldBeginPattern_ = true;

  size_t specialMode_ = 0;
#if JL_IS_CONFIG(FAIRY_WAND)
  OptionalMicroseconds overridePatternStartTime_;
#elif JL_IS_CONFIG(CLOUDS)
  StatusWatcher* statusWatcher_ = nullptr;  // Unowned.
  bool forceClouds_ = true;
  std::optional<CRGB> colorOverride_;
#endif

#if JL_IS_CONFIG(ORRERY_PLANET)
  PatternBits planetPattern_ = 0;
#endif  // ORRERY_PLANET

  NumLedWritesGetter* numLedWritesGetter_ = nullptr;
  NetworkManager& networkManager_;  // Unowned.

  OptionalMicroseconds lastLEDWriteTime_;

  Frame frame_;
  PredictableRandom predictableRandom_;
  XYIndexStore xyIndexStore_;

  Microseconds fpsEpochStart_ = 0;
  Microseconds timeSpentComputingEffectsThisEpoch_ = 0;
  uint32_t framesComputedThisEpoch_ = 0;

  bool isAllLinear_ = false;

  ProtocolEngine engine_{this};
};

std::string PatternName(PatternBits pattern, const Player& player);

}  // namespace jazzlights
#endif  // JL_RENDER_PLAYER_H
