#include "unisparks/effects/functional.hpp"
#include <math.h>
using namespace unisparks;
using namespace std;

static auto PULSE = effect([](const Frame& frame) {
  using unisparks::min;
  int w = width(frame);
  int h = height(frame);

  auto cc = center(frame);
  auto rr = min(w, h);
  auto r = rr*(0.2 + 0.2*pulse(frame));

  return [ = ](const Pixel& px) -> Color {
    auto d = distance(px.coord, cc);
    return d > r ? Color(RgbaColor(0,0,0,128)) : GREEN;
  };
});

const Effect* OVERLAYS[] = {
  &PULSE
};
int OVERLAYS_CNT = sizeof(OVERLAYS)/sizeof(*OVERLAYS);

