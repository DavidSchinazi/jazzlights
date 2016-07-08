#ifndef DFSPARKS_PALETTE_H
#define DFSPARKS_PALETTE_H
#include <stdint.h>

namespace dfsparks {

struct Palette16 {
  const static int size = 16;
  uint32_t colors[size];
};

extern const Palette16 RAINBOW_COLORS;
extern const Palette16 RAINBOW_STRIPE_COLORS;
extern const Palette16 PARTY_COLORS;
extern const Palette16 HEAT_COLORS;

} // namespace dfsparks
#endif /* DFSPARKS_PALETTE_H */
