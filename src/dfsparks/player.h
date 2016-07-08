#ifndef DFSPARKS_PLAYER_H
#define DFSPARKS_PLAYER_H
#include "dfsparks/effects.h"
#include "dfsparks/network.h"
#include "dfsparks/pixels.h"
#include <stdio.h>

namespace dfsparks {

class Player {
public:
  Player(Pixels &px);
  virtual ~Player();

  void begin();
  void end();

  void render();

  void next();
  void prev();

  void repeat();
  void loop();
  void shuffle();

  void play(Effect &ef);
  void play(int efi);

  void knock();

  Effect &get_playing_effect();

protected:
  Effects effects;
  Pixels &pixels;
  int32_t next_time;

private:
  virtual void on_playing(Effect &) {}

  enum PlaybackMode { repeat_one, loop_all, shuffle_all } pbmode = loop_all;
  Effect *playing;
};

class NetworkPlayer : public Player {
public:
  NetworkPlayer(Pixels &p, Network &n);

  using Player::begin;
  void render();
  void serve();
  void listen();
  void disconnect();

private:
  enum Mode { offline, client, server };

  void on_playing(Effect &ef) override;

  Network &netwrk;
  Mode mode = client;
};

} // namespace dfsparks
#endif /* DFSPARKS_PLAYER_H_ */
