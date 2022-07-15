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

  virtual size_t contextSize(const Frame& frame) const = 0;
  virtual Color color(const Frame& frame, const Pixel& px) const = 0;
  virtual void begin(const Frame& frame) const = 0;
  virtual void rewind(const Frame& frame) const = 0;
  virtual std::string effectName(PatternBits pattern) const = 0;
};

// TODO import some 2D patterns from Sound Reactive WLED
// https://github.com/scottrbailey/WLED-Utils/blob/main/effects_sr.md
// https://github.com/atuline/WLED/blob/master/wled00/FX.cpp

} // namespace unisparks
#endif /* UNISPARKS_EFFECT_H */
