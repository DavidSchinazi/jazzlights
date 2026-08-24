#ifndef JL_RENDER_PLAYER_H
#define JL_RENDER_PLAYER_H

#include <vector>

#include "jazzlights/effect/effect.h"
#include "jazzlights/layout/layout.h"
#include "jazzlights/network/network.h"
#include "jazzlights/protocol/engine.h"
#include "jazzlights/render/predictable_random.h"
#include "jazzlights/render/renderer.h"
#include "jazzlights/render/xy_index.h"
#include "jazzlights/util/types.h"

namespace jazzlights {

// Player renders patterns onto strands. The mechanics of the synchronization protocol - leader election, the
// originator table, and which pattern everyone agrees to display - live in ProtocolEngine, which Player owns and
// feeds with the messages it receives from its networks.
class Player : private ProtocolEngine::Delegate {
 public:
  Player();
  ~Player();

  // Disallow copy, allow move
  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

  // Constructing the player
  Player& addStrand(const Layout& l, Renderer& r);
  Player& connect(Network* n);

  /**
   * Prepare for rendering
   *
   * Call this when you're done adding strands, setting up
   * player configuration and connecting networks.
   */
  void begin();

  /**
   *  Render current frame to all strands.
   *  Returns whether the caller should send data to the LEDs.
   */
  bool render();

  /**
   *  Play next effect in the playlist
   */
  void next();

  /**
   *  Loop current effect forever.
   */
  void loopOne() { engine_.LoopOne(); }

  /**
   *  Stop looping current effect forever.
   */
  void stopLooping() { engine_.StopLooping(); }

  /**
   *  Returns whether we are looping current effect forever.
   */
  bool isLooping() const { return engine_.isLooping(); }

  /**
   *  Sets the current pattern and correspondingly resets the next pattern.
   */
  void setPattern(PatternBits pattern);

  /**
   *  Forces the rotation to always use this palette.
   */
  void forcePalette(uint8_t palette);

  /**
   *  Cancels previous call to forcePalette.
   */
  void stopForcePalette() { engine_.StopForcePalette(); }

  /**
   * Returns whether the palette is currently forced.
   */
  bool paletteIsForced() const { return engine_.paletteIsForced(); }

  /**
   * Returns the currently forced palette.
   */
  uint8_t forcedPalette() const { return engine_.forcedPalette(); }

  /**
   * Run text command
   */
  const char* command(const char* cmd);

  /**
   * Computes FPS information and resets counters.
   */
  void GenerateFPSReport(uint16_t* fpsCompute, uint16_t* fpsWrites, uint8_t* utilization,
                         Microseconds* timeSpentComputingThisEpoch, Microseconds* epochDuration);

  /**
   * Returns the bounding box of all pixels
   */
  const Box& bounds() const { return frame_.viewport; }

  void handleSpecial();
  void stopSpecial();
  size_t getSpecial() const { return specialMode_; }
#if JL_IS_CONFIG(FAIRY_WAND)
  void triggerPatternOverride();
#endif  // FAIRY_WAND

  bool is_power_limited() const { return powerLimited_; }
  void set_power_limited(bool powerLimited) { powerLimited_ = powerLimited; }
  uint8_t brightness() const { return brightness_; }
  void set_brightness(uint8_t brightness);

  void setBasePrecedence(Precedence basePrecedence) { engine_.SetBasePrecedence(basePrecedence); }
  void setPrecedenceGain(Precedence precedenceGain) { engine_.SetPrecedenceGain(precedenceGain); }
  void updatePrecedence(Precedence basePrecedence, Precedence precedenceGain);
  void setRandomizeLocalDeviceId(bool val) { engine_.SetRandomizeLocalDeviceId(val); }

  PredictableRandom* predictableRandom() { return &predictableRandom_; }
  PatternBits currentEffect() const;
  std::string currentEffectName() const;
  NetworkType following() const { return engine_.following(); }
  NumHops currentNumHops() const { return engine_.currentNumHops(); }

  bool enabled() const { return enabled_; }
  void set_enabled(bool enabled);

#if JL_AUDIO_VISUALIZER
  enum class SoundReactiveMode { kOff, kOn, kAuto };
  SoundReactiveMode sound_reactive_mode() const { return sound_reactive_mode_; }
  void set_sound_reactive_mode(SoundReactiveMode mode);
  bool sound_reactive_enabled() const;
#endif  // JL_AUDIO_VISUALIZER

#if JL_IS_CONFIG(CLOUDS)
  class StatusWatcher {
   public:
    virtual ~StatusWatcher() = default;
    virtual void OnStatus() = 0;
  };
  void set_status_watcher(StatusWatcher* status_watcher) { status_watcher_ = status_watcher; }
  void enable_color_override(CRGB color_override) {
    color_overridden_ = true;
    color_override_ = color_override;
  }
  void disable_color_override() { color_overridden_ = false; }
  bool color_overridden() const { return color_overridden_; }
  CRGB color_override() const { return color_override_; }
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

  // Hands the message the engine wants to advertise to every network, skipping the one we heard it from.
  void SendPendingMessage();
  void TriggerSendAsap();

#if JL_IS_CONFIG(CLOUDS) && !JL_DEV
  bool enabled_ = false;
#else   // JL_IS_CONFIG(CLOUDS) && !JL_DEV
  bool enabled_ = true;
#endif  // JL_IS_CONFIG(CLOUDS) && !JL_DEV

#if JL_AUDIO_VISUALIZER
  SoundReactiveMode sound_reactive_mode_ = SoundReactiveMode::kAuto;
  bool sound_reactive_suppressed_ = false;
  OptionalMicroseconds squelch_start_time_;
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
  StatusWatcher* status_watcher_ = nullptr;  // Unowned.
  bool color_overridden_ = false;
  bool force_clouds_ = true;
  CRGB color_override_;
#endif

#if JL_IS_CONFIG(ORRERY_PLANET)
  PatternBits planetPattern_ = 0;
#endif  // ORRERY_PLANET

  NumLedWritesGetter* numLedWritesGetter_ = nullptr;
  std::vector<Network*> networks_;

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

std::string patternName(PatternBits pattern, const Player& player);

}  // namespace jazzlights
#endif  // JL_RENDER_PLAYER_H
