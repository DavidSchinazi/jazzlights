#include "dfsparks/effect.h"

namespace dfsparks {

void Effect::begin(Pixels &px) {
  pixels = &px;
  start_time = frame_time = time_ms();
  beat_time = -1;
  on_begin(*pixels);
}

void Effect::end() {
  on_end(*pixels);
  pixels = nullptr;
}

void Effect::restart() { start_time = time_ms(); }

void Effect::knock() { beat_time = time_ms(); }

void Effect::sync(int32_t start_t, int32_t beat_t) {
  start_time = start_t;
  beat_time = beat_t;
}

void Effect::render() {
  frame_time = time_ms();
  on_render(*pixels);
}

Effect &Effect::set_name(const char *n) {
  name = n;
  return *this;
}

} // namespace dfsparks
