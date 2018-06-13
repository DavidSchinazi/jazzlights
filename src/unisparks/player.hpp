#ifndef UNISPARKS_PLAYER_H
#define UNISPARKS_PLAYER_H
#include "unisparks/effect.hpp"
#include "unisparks/layout.hpp"
#include "unisparks/network.hpp"
#include "unisparks/renderer.hpp"
#include "unisparks/renderers/simple.hpp"

namespace unisparks {

struct Playlist {

  struct Item {
    const Effect* effect;
    const char* name;
  };

  const Item* items;
  size_t size;
};

struct Strand {
  const Layout* layout;
  Renderer* renderer;
};

struct PlayerOptions {
  const Playlist* playlist = nullptr;
  Strand* strands = nullptr;
  size_t strandCount = 0;
  Network* network = nullptr;
  Milliseconds preferredEffectDuration = 10 * ONE_SECOND;
  int throttleFps = 30;
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
   *  Play effect with given index
   */
  void play(int idx);

  /**
   *  Play previous effect
   */
  void prev();

  /**
   *  Play next effect
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
   *  Overlay given effect on top of currently playing effect.
   *  If there was another overlay, it will be replaced with the new one.
   */
  void overlay(const Effect& ef);

  /**
   *  Remove overlayed effect if one was set up
   */
  void clearOverlay();

  /**
   * Play sync test pattern.
   *
   * Sync test is an overlay, so it will replace currently playing
   * overlay pattern if any
   */
  void syncTest(Milliseconds t);

  // State info
  int effectIndex() const {
    return track_;
  }

  int effectCount() const {
    return options_.playlist->size;
  }

  const char* effectName() const;

  Milliseconds effectTime() const {
    return time_;
  }

  BeatsPerMinute effectTempo() const {
    return tempo_;
  }

  bool paused() const {
    return paused_;
  }

  bool connected() const {
    return options_.network && options_.network->status() == CONNECTED;
  }

  int fps() const {
    return fps_;
  }

  const Effect* overlay() const {
    return overlay_;
  }

 private:
  void logStatus() const;
  int indexForName(const char* name) const;

  void rewind(const char* name, Milliseconds time);
  void rewind(int index, Milliseconds time);
  void sync();
  void render(const Effect& effect, Milliseconds time, BeatsPerMinute tempo,
              Milliseconds timeSinceDownbeat, Metre metre, const Layout& layout,
              Renderer* renderer);

  PlayerOptions options_;

  const Effect* overlay_ = nullptr;

  int track_ = -1;
  Milliseconds time_ = 0;
  Milliseconds overlayTime_ = 0;

  BeatsPerMinute tempo_ = 120;
  Metre metre_ = SIMPLE_QUADRUPLE;
  Milliseconds lastDownbeatTime_ = 0;

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
