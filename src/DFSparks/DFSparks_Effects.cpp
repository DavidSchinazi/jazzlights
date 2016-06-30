#include <DFSparks_Effects.h>

namespace dfsparks {

void Effects::begin(Pixels &pixels) {
  for (int i = 0; i < count(); ++i) {
    get(i)->begin(pixels);
  }
}

void Effects::end(Pixels &pixels) {
  for (int i = 0; i < count(); ++i) {
    get(i)->end(pixels);
  }
}

Effect *Effects::get(int32_t id) {
  switch (id) {
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
int Effects::count() const { return 7; }

} // namespace dfsparks
