#ifndef DFSPARKS_EFFECTS_H
#define DFSPARKS_EFFECTS_H
#include <DFSparks_Effect_Blink.h>
#include <DFSparks_Effect_Flame.h>
#include <DFSparks_Effect_Plasma.h>
#include <DFSparks_Effect_Rainbow.h>
#include <DFSparks_Effect_Rider.h>
#include <DFSparks_Effect_Threesine.h>

namespace dfsparks {

// Need to templatize on the number of pixels because some effects
// require per-pixel buffers and we don't want to allocate them
// dynamically
template <typename Pixels> class Effects {
public:
  Effect<Pixels> *operator[](int32_t number) {
    switch (number) {
    case 0:
      return &slow_blink;
    case 1:
      return &fast_blink;
    case 2:
      return &radiate_rainbow;
    case 3:
      return &threesine;
    case 4:
      return &plasma;
    case 5:
      return &rider;
    case 6:
      return &flame;
    default:
      return nullptr;
    }
  }

  int size() { return 7; }

  // Blink<Pixels> slow_blink = {1000, 0xff0000, 0x00ff00};
  Blink<Pixels> slow_blink = {1000, rgb(255, 0, 0), rgb(0, 255, 0)};
  Blink<Pixels> fast_blink = {75, 0xff0000, 0x00ff00};
  Rainbow<Pixels> radiate_rainbow = {1000};
  Threesine<Pixels> threesine = {15};
  Plasma<Pixels> plasma = {30};
  Rider<Pixels> rider = {30};
  Flame<Pixels> flame = {};
  // static auto fast_blink = Blink<Pixels>(75, 0xff0000, 0x00ff00);
  // Flame<Pixels> flame;
  // Rainbow<Pixels> rainbow_horizontal(1000, Rainbow::HORIZONTAL);
  // Rainbow rainbow_vertical(1000, Rainbow::VERTICAL);
  // Colorfill colorfill1(300, HEAT_COLORS, 2);
};

} // namespace dfsparks
#endif /* DFSPARKS_EFFECTS_H */