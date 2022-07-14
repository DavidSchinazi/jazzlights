#include "unisparks/frame.hpp"

namespace unisparks {

constexpr int noteDivider(Metre m) {
  return (m & 0x00FF);
}

constexpr int beatsPerBar(Metre m) {
  return (m & 0xFF00) >> 8;
}

constexpr Milliseconds beatDuration(BeatsPerMinute tempo) {
  return ONE_MINUTE / tempo; 
}

constexpr Milliseconds barDuration(BeatsPerMinute tempo, Metre metre) {
  return beatDuration(tempo) * beatsPerBar(metre); 
}

Milliseconds adjustDuration(const Frame& f, Milliseconds d) {
  Milliseconds btd = beatDuration(f.tempo);
  if (d < btd) {
    return btd;
  }
  Milliseconds brd = barDuration(f.tempo, f.metre);
  if (d < brd) {
    return btd * int(d/btd);
  }
  return brd * int(d/brd);
}

} // namespace unisparks

