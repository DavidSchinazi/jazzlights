#ifndef DFSPARKS_PLAYER_H
#define DFSPARKS_PLAYER_H
#include "dfsparks/network.h"
#include "dfsparks/pixels.h"
#include "dfsparks/playlist.h"
#include <assert.h>
#include <stdio.h>

namespace dfsparks {

class Player {
public:
  static constexpr int LOW_PRIORITY = 10;
  static constexpr int HIGH_PRIORITY = 20;

  Player(Pixels& pixels);
  virtual ~Player();

  void render();

  void play(int track, int priority = HIGH_PRIORITY) {doPlay(playlist_.select(track), priority);}
  void next(int priority = HIGH_PRIORITY) { doPlay(playlist_.next(), priority); }
  void prev(int priority = HIGH_PRIORITY) { doPlay(playlist_.prev(), priority); }

  int track() const { return playlist_.currentPosition(); }

  void cycleAll() { mode_ = cycle_all; }
  void shuffleAll() { mode_ = shuffle_all; }
  void loopOne() { mode_ = play_forever; }

  void knock() { time_since_beat_ = 0; }

  const char *effectName() const;
  int32_t timeElapsed() const { return elapsed_time_; }
  int32_t timeSinceBeat() const { return time_since_beat_; }
  int32_t cycleDuration() const {return 500; }
  uint8_t cycleHue() const { return cycle_hue_; }


  Pixels &pixels() {
    assert(pixels_);
    return *pixels_;
  }
  const Pixels &pixels() const {
    assert(pixels_);
    return *pixels_;
  }

  void showStatus(bool show = true) { showStatus_ = show; }
  bool isShowingStatus() const { return showStatus_; }

protected:
  Effect* findEffect(const char *name) const;
  
  void doPlay(Effect &ef, int priority);
  void doPlay(Effect &ef, int priority, int32_t elapsed, int32_t remaining, uint8_t cycleHue);

private:
  virtual void doRenderStatus();

  Repertoire repertoire_;
  Playlist playlist_;
  Effect *effect_ = nullptr;
  int32_t elapsed_time_;
  int32_t remaining_time_;
  int32_t time_since_beat_;
  int32_t frame_ts_;
  uint8_t cycle_hue_;
  int priority_ = LOW_PRIORITY;

  enum Mode { play_forever, cycle_all, shuffle_all } mode_ = cycle_all;
  Pixels *pixels_ = nullptr;
  bool showStatus_ = false;
};

class NetworkPlayer : public Player, NetworkListener {
public:
  NetworkPlayer(Pixels& pixels, Network &n);
  ~NetworkPlayer();

  void render();

  void setMaster() { mode_ = MASTER; }
  void setSlave() { mode_ = SLAVE; }
  void setStandalone() { mode_ = STANDALONE; }
  bool isMaster() const {return mode_ == MASTER;}
  bool isConnected() const {
    return mode_ != STANDALONE && netwrk.status() == Network::connected;
  }

  // deprecated, use setMaster() instead
  void serve() {setMaster();}

private:
  void onReceived(Network &network, const Message::Frame &frame) final;
  void onStatusChange(Network &) final{};
  void doRenderStatus() final;

  enum Role {STANDALONE, SLAVE, MASTER} mode_ = SLAVE;
  Network &netwrk;

  int32_t tx_time_ = INT32_MIN / 2;
  int32_t tx_interval_ = 500; // 250;
  int32_t tx_track_ = -1;

  int32_t rx_time_ = INT32_MIN / 2;
  int32_t rx_timeout_ = 3000;
};

} // namespace dfsparks
#endif /* DFSPARKS_PLAYER_H_ */
