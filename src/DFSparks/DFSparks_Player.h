#ifndef DFSPARKS_PLAYER_H
#define DFSPARKS_PLAYER_H
#include <DFSparks_Effects.h>
#include <DFSparks_Pixels.h>
#include <DFSparks_Timer.h>
#include <stdio.h>

namespace dfsparks {

class Player : private Pixels {
public:
  void begin() { effects.begin(*this); }

  void end() { effects.end(*this); }

  void render() { effects.get(effect_idx)->render(*this); }

  void play(int index) {
    int new_idx =
        (index < 0 ? effects.count() - index : index) % effects.count();
    if (new_idx != effect_idx) {
      effects.get(effect_idx)->sync(time_ms(), -1);
    }
    effect_idx = new_idx;
  }

  void next() { play(effect_idx + 1); }

  void prev() { play(effect_idx - 1); }

  int get_playing_effect() const { return effect_idx; }

private:
  int effect_idx = 2;
  Effects effects;
};

} // namespace dfsparks
#endif /* DFSPARKS_PLAYER_H_ */
