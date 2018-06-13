#ifndef UNISPARKS_RHYTM_HPP
#define UNISPARKS_RHYTM_HPP
#include "unisparks/util/time.hpp"
namespace unisparks {

using BeatsPerMinute = int32_t;
using Millibeats = uint32_t;

enum Metre {
  SIMPLE_QUADRUPLE = 0x0404,
  SIMPLE_TRIPLE = 0x0304,
};

} // namespace unisparks
#endif /* UNISPARKS_RHYTM_HPP */

