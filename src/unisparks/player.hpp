#ifndef UNISPARKS_PLAYER_H
#define UNISPARKS_PLAYER_H
#include "unisparks/effect.hpp"
#include "unisparks/layout.hpp"
#include "unisparks/network.hpp"
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

  /**
   * Reset player to the same state as if it was just constructed
   */
  void reset();

  // Constructing the player
  
  Player& clearStrands();
  Player& addStrand(const Layout& l, Renderer& r);
  Player& addStrand(const Layout& l, SimpleRenderFunc r);
  Player& connect(Network* n);

  /**
   * Prepare for rendering
   *
   * Call this when you're done adding strands and setting up
   * player configuration.
   */
  void begin();

  /**
   * Exit rendering mode and cleanup runtime resources
   */
  void end();

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

  bool powerLimited;


  void setBasePrecedence(Precedence basePrecedence) {basePrecedence_ = basePrecedence; }
  void setIncomingPrecedenceGain(Precedence precedenceGain) {incomingPrecedenceGain_ = precedenceGain; }
  void setOutgoingPrecedenceGain(Precedence precedenceGain) {outgoingPrecedenceGain_ = precedenceGain; }

 private:
  void syncToNetwork(Milliseconds currentTime);
  void nextInner(Milliseconds currentTime);
  Frame effectFrame(const Effect* effect, Milliseconds currentTime);
  Effect* currentEffect() const;
  void updateToNewPattern(PatternBits newCurrentPattern,
                          PatternBits newNextPattern,
                          Milliseconds elapsedTime,
                          Milliseconds currentTime);
  void render(NetworkStatus networkStatus, Milliseconds timeSinceLastRender, Milliseconds currentTime);
  void handleReceivedMessage(NetworkMessage message, Milliseconds currentTime);

  Precedence getLocalPrecedence(Milliseconds currentTime);
  Precedence getLeaderPrecedence(Milliseconds currentTime);
  Precedence getOutgoingLocalPrecedence(Milliseconds currentTime);
  Precedence getOutgoingLeaderPrecedence(Milliseconds currentTime);
  Precedence getOutgoingPrecedence(Milliseconds currentTime);
  Precedence getIncomingLocalPrecedence(Milliseconds currentTime);
  Precedence getIncomingLeaderPrecedence(Milliseconds currentTime);
  Precedence getIncomingPrecedence(Milliseconds currentTime);
  NetworkDeviceId getLocalDeviceId(Milliseconds currentTime);
  NetworkDeviceId getLeaderDeviceId(Milliseconds currentTime);
  NetworkDeviceId getFollowedDeviceId(Milliseconds currentTime);
  void abandonLeader(Milliseconds currentTime);

  bool ready_;

  struct Strand {
    const Layout* layout;
    Renderer* renderer;
  };

  Strand strands_[255];
  size_t strandCount_;

  Box viewport_;
  void* effectContext_ = nullptr;
  size_t effectContextSize_ = 0;

  Milliseconds elapsedTime_ = 0; // Time since start of this pattern.
  PatternBits currentPattern_;
  PatternBits nextPattern_;

  BeatsPerMinute tempo_;
  Metre metre_;
  // Milliseconds lastDownbeatTime_;

  bool loop_ = false;
  uint8_t specialMode_ = START_SPECIAL;

  std::vector<Network*> networks_;

  Milliseconds lastRenderTime_;

  Milliseconds lastLEDWriteTime_;
  Milliseconds lastUserInputTime_;
  bool followingLeader_;
  Network* followedNetwork_;
  NetworkDeviceId leaderDeviceId_;
  Precedence leaderPrecedence_;
  Milliseconds lastLeaderReceiveTime_;
  Precedence basePrecedence_ = 0;
  Precedence incomingPrecedenceGain_ = 0;
  Precedence outgoingPrecedenceGain_ = 0;

  // Mutable because it is used for logging
  mutable int fps_;
  mutable Milliseconds lastFpsProbeTime_;
  mutable int framesSinceFpsProbe_;
};


} // namespace unisparks
#endif /* UNISPARKS_PLAYER_H */
