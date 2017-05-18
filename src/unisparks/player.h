#ifndef UNISPARKS_PLAYER_H
#define UNISPARKS_PLAYER_H
#include "unisparks/network.h"
#include "unisparks/pixels.h"
#include "unisparks/playlists/default.h"
#include "unisparks/effects/system.h"
#include <assert.h>
#include <stdio.h>

namespace unisparks {

class Player {
public:
  Player(Pixels& pixels, const Playlist& playlist);
  Player(Player&) = delete;
  virtual ~Player();

  Player& operator=(Player&) = delete;

  void render();

  void play(int track);
  void next();
  void prev(); 

  int track() const { return track_; }

  void cycleAll() { mode_ = cycle_all; }
  void cycleOne() { mode_ = cycle_one; }
  void shuffleAll() { mode_ = shuffle_all; }
  void playForever() { mode_ = play_forever; }

  void knock() { frame_.timeSinceBeat = 0; }

  const char *effectName() const;

  // deprecated, use cycleOne
  void loopOne() {cycleOne();}

protected:
  const Playlist& playlist_;

  static constexpr int LOW_PRIORITY = 10;
  static constexpr int HIGH_PRIORITY = 20;

  void play(const Effect *ef, int priority);
  void sync(const Effect& ef, int priority, Frame fr);
  void advance();
  const Frame &frame() const { return frame_; }

private:
  enum Mode { play_forever, cycle_one, cycle_all, shuffle_all };

  Pixels& pixels_;

  Frame frame_ = {0, INT32_MAX/2, INT32_MAX/2};
  int32_t frameTs_ = 0;
  int track_=  0;
  int priority_ = LOW_PRIORITY;
  Effect::Renderer *renderer_ = nullptr;
  Mode mode_ = cycle_all;
  uint8_t *renderBuf_;
  size_t renderBufSize_;
};

class NetworkPlayer : public Player, NetworkListener {
public:
  NetworkPlayer(Pixels& px, Network &n, const Playlist& pl = defaultPlaylist());
  ~NetworkPlayer();

  void showStatus();

  void setMaster() { mode_ = MASTER; }
  void setSlave() { mode_ = SLAVE; }
  void setStandalone() { mode_ = STANDALONE; }
  bool isMaster() const {return mode_ == MASTER;}
  bool isConnected() const {
    return mode_ != STANDALONE && netwrk.status() == Network::connected;
  }

  void render();

  UNISPARKS_DEPRECATED("bool argument no longer supported, use showStatus() without parameters")
  void showStatus(bool) UNISPARKS_DEPRECATED_S;
    
  UNISPARKS_DEPRECATED("check effectName() instead")
  bool isShowingStatus() const UNISPARKS_DEPRECATED_S;

  UNISPARKS_DEPRECATED("use setMaster() instead")
  void serve() UNISPARKS_DEPRECATED_S;

private:
  void onReceived(Network &network, const NetworkFrame &frame) final;
  void onStatusChange(Network &) final{};
  void doRenderStatus();

  enum Role {STANDALONE, SLAVE, MASTER} mode_ = SLAVE;
  Network &netwrk;

  int32_t tx_time_ = INT32_MIN / 2;
  int32_t tx_interval_ = 500; // 250;
  int32_t tx_track_ = -1;

  int32_t rx_time_ = INT32_MIN / 2;
  int32_t rx_timeout_ = 3000;

  bool showStatus_ = false;
  NetworkStatus networkStatusEffect_;
};

} // namespace unisparks
#endif /* UNISPARKS_PLAYER_H_ */
