#include "dfsparks/effects.h"
#include "dfsparks/effects/blink.h"
#include "dfsparks/effects/flame.h"
#include "dfsparks/effects/glitter.h"
#include "dfsparks/effects/plasma.h"
#include "dfsparks/effects/rainbow.h"
#include "dfsparks/effects/rider.h"
#include "dfsparks/effects/slantbars.h"
#include "dfsparks/effects/threesine.h"
#include <assert.h>

namespace dfsparks {

constexpr int effect_cnt = 8;

void Effects::begin(Pixels &pixels) {
  // life is hard without STL :(
  int i = 0;
  all = new Effect *[effect_cnt];
  all[i++] = &(new Blink())->set_name("slowblink");
  all[i++] = &(new Rainbow())->set_name("radiaterainbow");
  all[i++] = &(new Threesine())->set_name("threesine");
  all[i++] = &(new Plasma())->set_name("plasma");
  all[i++] = &(new Rider())->set_name("rider");
  all[i++] = &(new Flame())->set_name("flame");
  all[i++] = &(new Glitter())->set_name("glitter");
  all[i++] = &(new Slantbars())->set_name("slantbars");
  assert(i == effect_cnt);
  for (i = 0; i < effect_cnt; ++i) {
    all[i]->begin(pixels);
  }
}

void Effects::end() {
  for (int i = 0; i < effect_cnt; ++i) {
    all[i]->end();
    delete all[i];
  }
  delete[] all;
  all = nullptr;
}

Effect *Effects::get(int idx) {
  if (idx >= 0 && idx < effect_cnt) {
    return all[idx];
  }
  return nullptr;
}

Effect *Effects::get(const char *name) {
  for (int i = 0; i < effect_cnt; ++i) {
    if (!strcmp(name, all[i]->get_name())) {
      return all[i];
    }
  }
  return nullptr;
}

Effect &Effects::first() { return *all[0]; }

Effect *Effects::next(const Effect &ef) {
  for (int i = 0; i < effect_cnt; ++i) {
    if (all[i] == &ef) {
      return i < effect_cnt - 1 ? all[i + 1] : all[0];
    }
  }
  return nullptr;
}

Effect *Effects::prev(const Effect &ef) {
  for (int i = 0; i < effect_cnt; ++i) {
    if (all[i] == &ef) {
      return i > 0 ? all[i - 1] : all[effect_cnt - 1];
    }
  }
  return nullptr;
}

int Effects::count() const { return effect_cnt; }

} // namespace dfsparks
