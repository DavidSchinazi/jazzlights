#ifndef JL_PLAYER_H
#define JL_PLAYER_H

#include <vector>

#include "jazzlights/effect.h"
#include "jazzlights/layout.h"
#include "jazzlights/network.h"
#include "jazzlights/pseudorandom.h"
#include "jazzlights/registry.h"
#include "jazzlights/renderer.h"

namespace jazzlights {

enum : Milliseconds {
  kEffectDuration = 10 * ONE_SECOND,
};

class Player {
 public:
  Player();
  ~Player();

  // Disallow copy, allow move
  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

  // Constructing the player
  Player& clearStrands();
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
   * Returns number of frames rendered per second.
   */
  uint32_t fps() const { return fps_; }

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

  bool IsPowerLimited() const { return powerLimited_; }
  void SetPowerLimited(bool powerLimited) { powerLimited_ = powerLimited; }
  uint8_t GetBrightness() const { return brightness_; }
  void SetBrightness(uint8_t brightness) { brightness_ = brightness; }

  void setBasePrecedence(Precedence basePrecedence) { basePrecedence_ = basePrecedence; }
  void setPrecedenceGain(Precedence precedenceGain) { precedenceGain_ = precedenceGain; }
  void updatePrecedence(Precedence basePrecedence, Precedence precedenceGain, Milliseconds currentTime);
  void setRandomizeLocalDeviceId(bool val) { randomizeLocalDeviceId_ = val; }

  PredictableRandom* predictableRandom() { return &predictableRandom_; }
  PatternBits currentEffect() const;
  std::string currentEffectName() const;
  const Network* followedNextHopNetwork() const { return followedNextHopNetwork_; }
  NumHops currentNumHops() const { return currentNumHops_; }

 private:
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
    Network* nextHopNetwork = nullptr;
    NumHops numHops = 0;
    bool retracted = false;
    int8_t patternStartTimeMovementCounter = 0;
  };

  OriginatorEntry* getOriginatorEntry(NetworkDeviceId originator, Milliseconds currentTime);
  void checkLeaderAndPattern(Milliseconds currentTime);
  PatternBits enforceForcedPalette(PatternBits pattern);

  bool ready_ = false;
  bool powerLimited_ = false;
  uint8_t brightness_ = 255;

  struct Strand {
    const Layout* layout;
    Renderer* renderer;
  };

  Strand strands_[255];
  size_t strandCount_ = 0;

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
#endif  // FAIRY_WAND

  std::vector<Network*> networks_;
  std::list<OriginatorEntry> originatorEntries_;

  Milliseconds lastLEDWriteTime_ = -1;
  Milliseconds lastUserInputTime_ = -1;
  Precedence basePrecedence_ = 0;
  Precedence precedenceGain_ = 0;
  bool randomizeLocalDeviceId_ = false;
  NetworkDeviceId localDeviceId_ = NetworkDeviceId();
  NetworkDeviceId currentLeader_ = NetworkDeviceId();
  Network* followedNextHopNetwork_ = nullptr;
  NumHops currentNumHops_ = 0;

  Frame frame_;
  PredictableRandom predictableRandom_;
  XYIndexStore xyIndexStore_;

  bool paletteIsForced_ = false;
  uint8_t forcedPalette_ = 0;

  uint32_t fps_ = 0;
  Milliseconds lastFpsProbeTime_ = 0;
  uint64_t framesSinceFpsProbe_ = 0;
};

std::string patternName(PatternBits pattern);

}  // namespace jazzlights
#endif  // JL_PLAYER_H
