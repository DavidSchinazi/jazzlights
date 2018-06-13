#ifndef UNISPARKS_EFFECT_H
#define UNISPARKS_EFFECT_H
#include "unisparks/frame.hpp"
#include "unisparks/util/color.hpp"
#include "unisparks/util/geom.hpp"

namespace unisparks {

class Effect {
 public:
  virtual ~Effect() = default;

  virtual size_t contextSize(const Frame& frame) const = 0;
  virtual void begin(const Frame& frame) const = 0;
  virtual void end(const Frame& frame) const = 0;
  virtual Color color(Point pt, const Frame& frame) const = 0;
};


} // namespace unisparks
#endif /* UNISPARKS_EFFECT_H */
