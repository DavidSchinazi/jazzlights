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
  void render(NetworkStatus networkStatus, Milliseconds currentTime);

  /**
   *  Play next effect in the playlist
   */
  void next(Milliseconds currentTime);

  /**
   *  Loop current effect forever.
   */
  void loopOne();

  /**
   * Run text command
   */
  const char* command(const char* cmd);

  /**
   * Returns number of frames rendered per second.
   */
  int fps() const {
    return fps_;
  }

  /**
   * Returns the bounding box of all pixels
   */
  const Box& bounds() const {
    return viewport_;
  }

  void handleSpecial();
  void stopSpecial();

  bool powerLimited = false;


  void setBasePrecedence(Precedence basePrecedence) {basePrecedence_ = basePrecedence; }
  void setPrecedenceGain(Precedence precedenceGain) {precedenceGain_ = precedenceGain; }

  PredictableRandom& predictableRandom();

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

  Box viewport_;
  void* effectContext_ = nullptr;
  size_t effectContextSize_ = 0;

  Milliseconds currentPatternStartTime_ = 0;
  PatternBits currentPattern_;
  PatternBits nextPattern_;
  const Effect* lastBegunEffect_ = nullptr;

  bool loop_ = false;
  size_t specialMode_ = START_SPECIAL;

  std::vector<Network*> networks_;
  std::list<OriginatorEntry> originatorEntries_;

  Milliseconds lastLEDWriteTime_ = -1;
  Milliseconds lastUserInputTime_ = -1;
  Precedence basePrecedence_ = 0;
  Precedence precedenceGain_ = 0;
  NetworkDeviceId localDeviceId_ = NetworkDeviceId();
  NetworkDeviceId currentLeader_ = NetworkDeviceId();

  Frame frame_;
  PredictableRandom predictableRandom_;
  bool randomInitialized_ = false;

  // Mutable because it is used for logging
  mutable int fps_ = -1;
  mutable Milliseconds lastFpsProbeTime_ = -1;
  mutable int framesSinceFpsProbe_ = -1;
};


} // namespace unisparks
#endif /* UNISPARKS_PLAYER_H */
