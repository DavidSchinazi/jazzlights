#ifndef JL_PLAYER_H
#define JL_PLAYER_H

#include <vector>

#include "jazzlights/effect/effect.h"
#include "jazzlights/layout/layout.h"
#include "jazzlights/network/network.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/renderer.h"
#include "jazzlights/types.h"

namespace jazzlights {

class Player {
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
  void begin(Milliseconds currentTime);

  /**
   *  Render current frame to all strands.
   *  Returns whether the caller should send data to the LEDs.
   */
  bool render(Milliseconds currentTime);

  /**
   *  Play next effect in the playlist
   */
  void next(Milliseconds currentTime);

  /**
   *  Loop current effect forever.
   */
  void loopOne(Milliseconds currentTime);

  /**
   *  Stop looping current effect forever.
   */
  void stopLooping(Milliseconds currentTime);

  /**
   *  Returns whether we are looping current effect forever.
   */
  bool isLooping() const { return loop_; }

  /**
   *  Sets the current pattern and correspondingly resets the next pattern.
   */
  void setPattern(PatternBits pattern, Milliseconds currentTime);

  /**
   *  Forces the rotation to always use this palette.
   */
  void forcePalette(uint8_t palette, Milliseconds currentTime);

  /**
   *  Cancels previous call to forcePalette.
   */
  void stopForcePalette(Milliseconds currentTime);

  /**
   * Run text command
   */
  const char* command(const char* cmd);

  /**
   * Computes FPS information and resets counters.
   */
  void GenerateFPSReport(uint16_t* fpsCompute, uint16_t* fpsWrites, uint8_t* utilization,
                         Milliseconds* timeSpentComputingThisEpoch, Milliseconds* epochDuration);

  /**
   * Returns the bounding box of all pixels
   */
  const Box& bounds() const { return frame_.viewport; }

  void handleSpecial(Milliseconds currentTime);
  void stopSpecial(Milliseconds currentTime);
  size_t getSpecial() const { return specialMode_; }
#if JL_IS_CONFIG(FAIRY_WAND)
  void triggerPatternOverride(Milliseconds currentTime);
#endif  // FAIRY_WAND

  bool is_power_limited() const { return powerLimited_; }
  void set_power_limited(bool powerLimited) { powerLimited_ = powerLimited; }
  uint8_t brightness() const { return brightness_; }
  void set_brightness(uint8_t brightness);

  void setBasePrecedence(Precedence basePrecedence) { basePrecedence_ = basePrecedence; }
  void setPrecedenceGain(Precedence precedenceGain) { precedenceGain_ = precedenceGain; }
  void updatePrecedence(Precedence basePrecedence, Precedence precedenceGain, Milliseconds currentTime);
  void setRandomizeLocalDeviceId(bool val) { randomizeLocalDeviceId_ = val; }

  PredictableRandom* predictableRandom() { return &predictableRandom_; }
  PatternBits currentEffect() const;
  std::string currentEffectName() const;
  NetworkType following() const { return followedNextHopNetworkType_; }
  NumHops currentNumHops() const { return currentNumHops_; }

  bool enabled() const { return enabled_; }
  void set_enabled(bool enabled);
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
  void CloudNext(Milliseconds currentTime);
#endif  // CLOUDS

  class NumLedWritesGetter {
   public:
    virtual ~NumLedWritesGetter() = default;
    virtual uint32_t GetAndClearNumWrites() = 0;
  };
  void SetNumLedWritesGetter(NumLedWritesGetter* numLedWritesGetter) { numLedWritesGetter_ = numLedWritesGetter; }

 private:
  void UpdateStatusWatcher();
  void handleReceivedMessage(NetworkMessage message, Milliseconds currentTime);

  Precedence getLocalPrecedence(Milliseconds currentTime);

  struct OriginatorEntry {
    NetworkDeviceId originator = NetworkDeviceId();
    Precedence precedence = 0;
    PatternBits currentPattern = 0;
    PatternBits nextPattern = 0;
    Milliseconds currentPatternStartTime = 0;
    Milliseconds lastOriginationTime = 0;
    NetworkDeviceId nextHopDevice = NetworkDeviceId();
    NetworkId nextHopNetworkId = 0;
    NetworkType nextHopNetworkType = NetworkType::kLeading;
    NumHops numHops = 0;
    bool retracted = false;
    int8_t patternStartTimeMovementCounter = 0;
  };

  OriginatorEntry* getOriginatorEntry(NetworkDeviceId originator, Milliseconds currentTime);
  void checkLeaderAndPattern(Milliseconds currentTime);
  PatternBits enforceForcedPalette(PatternBits pattern);

#if JL_IS_CONFIG(CLOUDS) && !JL_DEV
  bool enabled_ = false;
#else   // JL_IS_CONFIG(CLOUDS) && !JL_DEV
  bool enabled_ = true;
#endif  // JL_IS_CONFIG(CLOUDS) && !JL_DEV

  bool ready_ = false;
  bool powerLimited_ = false;
  uint8_t brightness_ = 255;

  struct Strand {
    const Layout& layout;
    Renderer& renderer;
  };
  std::vector<Strand> strands_;

  void* effectContext_ = nullptr;
  size_t effectContextSize_ = 0;

  Milliseconds currentPatternStartTime_ = 0;
  PatternBits currentPattern_;
  PatternBits nextPattern_;
  PatternBits lastBegunPattern_ = 0;
  bool shouldBeginPattern_ = true;

  bool loop_ = false;
  size_t specialMode_ = 0;
#if JL_IS_CONFIG(FAIRY_WAND)
  Milliseconds overridePatternStartTime_ = -1;
#elif JL_IS_CONFIG(CLOUDS)
  StatusWatcher* status_watcher_ = nullptr;  // Unowned.
  bool color_overridden_ = false;
  bool force_clouds_ = true;
  CRGB color_override_;
#elif JL_IS_CONFIG(CREATURE)
  bool creatureIsFollowingNonCreature_ = false;
#endif

  NumLedWritesGetter* numLedWritesGetter_ = nullptr;
  std::vector<Network*> networks_;
  std::list<OriginatorEntry> originatorEntries_;

  Milliseconds lastLEDWriteTime_ = -1;
  Milliseconds lastUserInputTime_ = -1;
  Precedence basePrecedence_ = 0;
  Precedence precedenceGain_ = 0;
  bool randomizeLocalDeviceId_ = false;
  NetworkDeviceId localDeviceId_ = NetworkDeviceId();
  NetworkDeviceId currentLeader_ = NetworkDeviceId();
  NetworkId followedNextHopNetworkId_ = 0;
  NetworkType followedNextHopNetworkType_ = NetworkType::kLeading;
  NumHops currentNumHops_ = 0;

  Frame frame_;
  PredictableRandom predictableRandom_;
  XYIndexStore xyIndexStore_;

  bool paletteIsForced_ = false;
  uint8_t forcedPalette_ = 0;

  Milliseconds fpsEpochStart_ = 0;
  Milliseconds timeSpentComputingEffectsThisEpoch_ = 0;
  uint32_t framesComputedThisEpoch_ = 0;
};

std::string patternName(PatternBits pattern);

}  // namespace jazzlights
#endif  // JL_PLAYER_H
