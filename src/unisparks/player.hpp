#ifndef UNISPARKS_PLAYER_H
#define UNISPARKS_PLAYER_H
#include "unisparks/effect.hpp"
#include "unisparks/layout.hpp"
#include "unisparks/network.hpp"
#include "unisparks/renderer.hpp"
#include "unisparks/renderers/simple.hpp"

namespace unisparks {

struct Strand {
  const Layout* layout;
  Renderer* renderer;
};

struct PlayerOptions {
  Strand* strands = nullptr;
  size_t strandCount = 0;
  Network* network = nullptr;
  Milliseconds preferredEffectDuration = 10 * ONE_SECOND;
  int throttleFps = 30;
  const EffectInfo* customEffects = nullptr;
  size_t customEffectCount = 0;  
};

class Player {
 public:

  Player();
  ~Player();

  // Disallow copy, allow move
  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

  // Initializers
  void begin(const PlayerOptions options);
  void begin(const Layout& layout, Renderer& renderer);
  void begin(const Layout& layout, SimpleRenderFunc renderer);
  void begin(const Layout& layout, Renderer& renderer, Network& network);
  void begin(const Layout& layout, SimpleRenderFunc renderer, Network& network);

  /**
   *  Render current frame to all strands
   */
  void render();


  /**
   *  Render current frame to all strands
   */
  void render(Milliseconds dt);

  /**
   *  Play effect with given name
   */
  void play(const char* name);

  /**
   *  Overlay given effect on top of currently playing effect.
   *  If there was another overlay, it will be replaced with the new one.
   */
  void overlay(const char* name);

  /**
   *  Remove overlayed effect if one was set up
   */
  void clearOverlay();

  /**
   *  Play effect with given index in the playlist
   */
  void jump(int idx);

  /**
   *  Play previous effect in the playlist
   */
  void prev();

  /**
   *  Play next effect in the playlist
   */
  void next();

  /**
   *  Pause playback
   */
  void pause();

  /**
   *  Resume playback
   */
  void resume();

  /**
   * Returns total number of effects
   * player can play. This is regardless
   * of whether the effects are in the 
   * playlist. 
   */
  size_t effectCount() const {
    return effectCount_;
  }

  /**
   * Returns current playlist position 
   */
  size_t effectIndex() const {
    return track_;
  }

  /**
   * Returns the name of currently playing effect
   */
  const char* effectName() const;

  /**
   * Returns the name of currently playing overlay or "none"
   * if no overlay is playing.
   */
  const char* overlayName() const {
    return overlayIdx_ < effectCount_ ? effects_[overlayIdx_].name : "none";
  }

  /**
   * Returns time elapsed since effect start
   */
  Milliseconds effectTime() const {
    return time_;
  }

  /**
   * Returns current tempo
   */
  BeatsPerMinute tempo() const {
    return tempo_;
  }

  /**
   * Returns whether playback is paused
   */
  bool paused() const {
    return paused_;
  }

  /**
   * Returns whether player is connected to network
   */
  bool connected() const {
    return options_.network && options_.network->status() == CONNECTED;
  }

  /**
   * Returns number of frames rendered per second.
   */
  int fps() const {
    return fps_;
  }

 private:
  void syncToNetwork();
  bool syncEffectByName(const char* name, Milliseconds time);
  bool syncEffectByIndex(size_t index, Milliseconds time);
  bool switchToPlaylistItem(size_t index);
  void enrollEffect(const EffectInfo& ei);
  bool findEffect(const char *name, size_t* idx);
  Frame effectFrame() const;
  Frame overlayFrame() const;

  PlayerOptions options_;
  Box viewport_;
  void* effectContext_ = nullptr;
  void* overlayContext_ = nullptr;

  EffectInfo effects_[255];
  size_t effectCount_ = 0;

  size_t effectIdx_ = 0;
  Milliseconds time_ = 0;

  size_t overlayIdx_ = 0;
  Milliseconds overlayTime_ = 0;

  BeatsPerMinute tempo_ = 120;
  Metre metre_ = SIMPLE_QUADRUPLE;
  Milliseconds lastDownbeatTime_ = 0;

  uint8_t playlist_[255];
  size_t playlistSize_ = 0;
  size_t track_ = 0;

  bool paused_ = false;

  Milliseconds lastRenderTime_ = -1;

  // Mutable because it is used for logging
  mutable int fps_ = -1;
  mutable Milliseconds lastFpsProbeTime_ = -1;
  mutable int framesSinceFpsProbe_ = -1;

  // This is to simplify object construction in basic cases
  Strand firstStrand_;
  SimpleRenderer basicRenderer_;
};


} // namespace unisparks
#endif /* UNISPARKS_PLAYER_H */
