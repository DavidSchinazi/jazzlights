#ifndef UNISPARKS_EFFECT_FLAME_H
#define UNISPARKS_EFFECT_FLAME_H
#include "unisparks/effects/functional.hpp"
namespace unisparks {

class Flame : public XYIndexEffect<uint8_t> {
  public:
  void innerBegin(const Frame& frame) const override;
  void innerRewind(const Frame& frame) const override;
  Color innerColor(const Frame& frame, const Pixel& px) const override;
  std::string effectName(PatternBits /*pattern*/) const override {return "flame"; }
};

inline Flame flame() {
  return Flame();
}

} // namespace unisparks
#endif /* UNISPARKS_EFFECT_FLAME_H */
