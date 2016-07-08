#include "dfsparks/player.h"
#include <assert.h>
#include <stdlib.h>

namespace dfsparks {

// ==========================================================================
//  Player
// ==========================================================================
Player::Player(Pixels &px) : pixels(px) {}

Player::~Player() {}

void Player::begin() {
  effects.begin(pixels);
  playing = &effects.first();
  next_time = time_ms() + playing->get_preferred_duration();
}

void Player::end() { effects.end(); }

void Player::render() {
  if (playing) {
    playing->render();
  }
  int32_t curr_t = time_ms();
  if (curr_t > next_time) {
    switch (pbmode) {
    case loop_all:
      next();
      break;
    case shuffle_all:
      play(rand() % effects.count());
      break;
    case repeat_one:
      next_time = curr_t + playing->get_preferred_duration();
      break;
    }
  }
}

void Player::play(int idx) {
  info("player %p: play %d", this, idx);
  Effect *ef = effects.get(idx);
  if (ef) {
    play(*ef);
  }
}

void Player::play(Effect &ef) {
  playing = &ef;
  ef.restart();
  next_time = time_ms() + ef.get_preferred_duration();
  // info("player %p playing %s", this, ef.get_name());
  on_playing(ef);
}

void Player::next() {
  assert(playing);
  Effect *next = effects.next(*playing);
  assert(next);
  play(*next);
}

void Player::prev() {
  assert(playing);
  Effect *prev = effects.prev(*playing);
  assert(prev);
  play(*prev);
}

void Player::knock() {
  assert(playing);
  playing->knock();
}

Effect &Player::get_playing_effect() {
  assert(playing);
  return *playing;
}

// ==========================================================================
//  NetworkPlayer
// ==========================================================================
NetworkPlayer::NetworkPlayer(Pixels &p, Network &n) : Player(p), netwrk(n) {}

void NetworkPlayer::on_playing(Effect &ef) {
  if (mode == server) {
    PlayerState st = {ef.get_name(), ef.get_start_time(), ef.get_beat_time()};
    netwrk.broadcast(st);
  }
}

void NetworkPlayer::render() {
  if (mode == client) {
    PlayerState st;
    if (netwrk.sync(&st) >= 0) {
      if (strcmp(st.effect_name, get_playing_effect().get_name())) {
        play(*effects.get(st.effect_name));
      }
      get_playing_effect().sync(st.start_time, st.beat_time);
      next_time = get_playing_effect().get_preferred_duration();
    }
  } else if (mode == server) {
    Effect &ef = get_playing_effect();
    PlayerState st = {ef.get_name(), ef.get_start_time(), ef.get_beat_time()};
    netwrk.broadcast(st);
  }
  Player::render();
}

void NetworkPlayer::serve() { mode = server; }

void NetworkPlayer::listen() { mode = client; }

void NetworkPlayer::disconnect() { mode = offline; }

} // namespace dfsparks
