#include "unisparks/effects/functional.hpp"
#include <math.h>
using namespace unisparks;
using namespace std;

static auto PULSE = effect([](const Frame& frame) {
  using unisparks::min;
  int width = frame.size.width;
  int height = frame.size.height;

  auto cc = center(frame);
  auto rr = min(width, height);
  auto r = rr*(0.2 + 0.2*pulse(frame));

  return [ = ](Point p) -> Color {
    auto d = distance(p, cc);
    return d > r ? Color(RgbaColor(0,0,0,128)) : GREEN;
  };
});

const Effect* OVERLAYS[] = {
  &PULSE
};
int OVERLAYS_CNT = sizeof(OVERLAYS)/sizeof(*OVERLAYS);

