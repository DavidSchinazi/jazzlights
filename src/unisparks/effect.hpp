#ifndef UNISPARKS_EFFECT_H
#define UNISPARKS_EFFECT_H
#include "unisparks/frame.hpp"
#include "unisparks/util/color.hpp"
#include "unisparks/util/geom.hpp"
#include "unisparks/util/log.hpp"

#include <string>

namespace unisparks {

class Effect {
 public:
  virtual ~Effect() = default;

  virtual size_t contextSize(const Animation&) const = 0;
  virtual Color color(const Pixel& px) const = 0;
  virtual void begin(const Frame&) const = 0;
  virtual void rewind(const Frame& frame) const = 0;
  virtual std::string name() const = 0;
};

// TODO import some patterns from WLED
// https://github.com/Aircoookie/WLED/wiki/List-of-effects-and-palettes
// https://github.com/Aircoookie/WLED/blob/main/wled00/FX.cpp

} // namespace unisparks
#endif /* UNISPARKS_EFFECT_H */
