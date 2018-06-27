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

double pulse(const Frame& frame, double minval, double maxval,
                    int divider = 1, double upbeatK = 0.7) {
  auto btd = beatDuration(frame.tempo/divider);
  auto brd = barDuration(frame.tempo, frame.metre);
  auto beatWave = sin((frame.time % btd) * M_PI  / btd);
  auto barWave = sin((frame.time % brd) * M_PI  / brd);
  return minval + (maxval - minval) * (beatWave + upbeatK * barWave) /
         (upbeatK * 2);
}

double pulse(const Frame& frame) {
  return pulse(frame, -1, 1);
}

uint8_t cycleHue(const Frame& frame) {
  return 256 * frame.time / (4 * barDuration(frame.tempo, frame.metre));
}

uint8_t cycleHue(const Frame& frame, Milliseconds periodHint) {
  return 256 * frame.time / adjustDuration(frame, periodHint);  
}


} // namespace unisparks

