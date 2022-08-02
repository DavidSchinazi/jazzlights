#ifndef UNISPARKS_PLAYER_H
#define UNISPARKS_PLAYER_H
#include "unisparks/effect.hpp"
#include "unisparks/layout.hpp"
#include "unisparks/network.hpp"
#include "unisparks/pseudorandom.h"
#include "unisparks/renderer.hpp"
#include "unisparks/renderers/simple.hpp"
#include "unisparks/registry.hpp"

#include <vector>

#ifndef START_SPECIAL
#  define START_SPECIAL 0
#endif // START_SPECIAL

namespace unisparks {

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
  Player& addStrand(const Layout& l, SimpleRenderFunc r);
  Player& connect(Network* n);

  /**
   * Prepare for rendering
   *
   * Call this when you're done adding strands, setting up
   * player configuration and connecting networks.
   */
  void begin(Milliseconds currentTime);

  /**
   *  Render current frame to all strands
   */
  void render(Milliseconds currentTime);

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
   * Run text command
   */
  const char* command(const char* cmd);

  /**
   * Returns number of frames rendered per second.
   */
  uint32_t fps() const {
    return fps_;
  }

  /**
   * Returns the bounding box of all pixels
   */
  const Box& bounds() const {
    return frame_.viewport;
  }

  void handleSpecial(Milliseconds currentTime);
  void stopSpecial(Milliseconds currentTime);
  size_t getSpecial() const { return specialMode_; }
#if FAIRY_WAND
  void triggerPatternOverride(Milliseconds currentTime);
#endif  // FAIRY_WAND

  bool powerLimited = false;


  void setBasePrecedence(Precedence basePrecedence) {basePrecedence_ = basePrecedence; }
  void setPrecedenceGain(Precedence precedenceGain) {precedenceGain_ = precedenceGain; }
  void setRandomizeLocalDeviceId(bool val) { randomizeLocalDeviceId_ = val; }

  PredictableRandom* predictableRandom() { return &predictableRandom_; }
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

  bool ready_ = false;

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
  size_t specialMode_ = START_SPECIAL;
#if FAIRY_WAND
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

  uint32_t fps_ = 0;
  Milliseconds lastFpsProbeTime_ = 0;
  uint64_t framesSinceFpsProbe_ = 0;
};


} // namespace unisparks
#endif /* UNISPARKS_PLAYER_H */
