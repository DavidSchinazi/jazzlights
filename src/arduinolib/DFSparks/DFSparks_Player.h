#ifndef DFSPARKS_PLAYER_H
#define DFSPARKS_PLAYER_H
#include <stdio.h>

namespace dfsparks {

template <typename Pixels> class Player {
public:
  Player(Effects<Pixels> &ef)
      : effects(ef), effect_count(ef.size()), effect_idx(/*effects.size() - 1*/ 2) {};

  ~Player() { effects[effect_idx]->end(); }

  int get_playing_effect_id() const { return effect_idx; }

  int32_t get_elapsed_time() { return effects[effect_idx]->get_elapsed_time(); }
  void render(Pixels &pixels) { effects[effect_idx]->render(pixels); }

  void mirror(int effect_id, int32_t start_time) {
    if (effect_id >= 0 && effect_id != effect_idx) {
      play(effect_id);
      if (effect_idx == effect_id) {
        //effects[effect_idx]->sync_start_time(start_time);
      }
    }
  }

  void play(int index) {
    effects[effect_idx]->end();
    effect_idx = (index < 0 ? effect_count - index : index) % effect_count;
    effects[effect_idx]->begin();
  }

  void play_next() { play(effect_idx + 1); }

  void play_prev() { play(effect_idx - 1); }

private:
  Effects<Pixels> effects;
  int effect_count;
  int effect_idx;
};

} // namespace dfsparks
#endif /* DFSPARKS_PLAYER_H_ */
