#ifndef UNISPARKS_PALETTE_H
#define UNISPARKS_PALETTE_H
#include <stdint.h>

namespace unisparks {

struct Palette16 {
  const static int size = 16;
  uint32_t colors[size];
};

extern const Palette16 RAINBOW_COLORS;
extern const Palette16 RAINBOW_STRIPE_COLORS;
extern const Palette16 PARTY_COLORS;
extern const Palette16 HEAT_COLORS;

} // namespace unisparks
#endif /* UNISPARKS_PALETTE_H */
