#ifndef UNISPARKS_TYPES_H
#define UNISPARKS_TYPES_H

#include <cstdint>

#include "unisparks/util/geom.hpp"

namespace unisparks {

using PatternBits = uint32_t;
using Precedence = uint16_t;
using NumHops = uint8_t;

struct Pixel {
  int index;
  Point coord;
};

} // namespace unisparks

#endif // UNISPARKS_TYPES_H
