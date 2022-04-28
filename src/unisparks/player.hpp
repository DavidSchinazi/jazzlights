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
  Player& throttleFps(FramesPerSecond v);
  Player& connect(Network& n);

  /**
   * Prepare for rendering
   *
   * Call this when you're done adding strands and setting up
   * player configuration.
   */
  void begin();
  void begin(const Layout& layout, Renderer& renderer);
  void begin(const Layout& layout, SimpleRenderFunc renderer);
  void begin(const Layout& layout, Renderer& renderer, Network& network);
  void begin(const Layout& layout, SimpleRenderFunc renderer, Network& network);

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
   *  Pause playback
   */
  void pause();

  /**
   *  Resume playback
   */
  void resume();

  /**
   *  Loop current effect forever.
   */
  void loopOne();

  /**
   * Run text command
   */
  const char* command(const char* cmd);

  /**
   * Returns whether playback is paused
   */
  bool paused() const {
    return paused_;
  }

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

 private:
  void syncToNetwork(Milliseconds currentTime);
  void nextInner(Milliseconds currentTime);
  void reactToUserInput(Milliseconds currentTime);
  Frame effectFrame(const Effect* effect, Milliseconds currentTime);
  Effect* currentEffect() const;
  void updateToNewPattern(PatternBits newPattern, Milliseconds elapsedTime, Milliseconds currentTime);
  void render(NetworkStatus networkStatus, Milliseconds timeSinceLastRender, Milliseconds currentTime);

  Precedence GetLocalPrecedence(Milliseconds currentTime);
  Precedence GetLeaderPrecedence(Milliseconds currentTime);
  Precedence GetFollowedPrecedence(Milliseconds currentTime);
  NetworkDeviceId GetFollowedDeviceId();

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

  BeatsPerMinute tempo_;
  Metre metre_;
  // Milliseconds lastDownbeatTime_;

  bool paused_;
  bool loop_ = false;
  uint8_t specialMode_ = START_SPECIAL;

  std::vector<Network*> networks_;

  int throttleFps_;

  Milliseconds lastRenderTime_;

  Milliseconds lastLEDWriteTime_;
  Milliseconds lastUserInputTime_;
  bool followingLeader_;
  NetworkDeviceId leaderDeviceId_;
  Precedence leaderPrecedence_;
  Milliseconds lastLeaderReceiveTime_;

  // Mutable because it is used for logging
  mutable int fps_;
  mutable Milliseconds lastFpsProbeTime_;
  mutable int framesSinceFpsProbe_;
};


} // namespace unisparks
#endif /* UNISPARKS_PLAYER_H */
