#ifndef UNISPARKS_EFFECTS_TRANSFORM_H
#define UNISPARKS_EFFECTS_TRANSFORM_H
#include "unisparks/effect.hpp"

namespace unisparks {


template<typename T>
struct Transformed : Effect {

  Transformed(const Transform& tr, const T& ef) : transform(tr), effect(ef) {
  }

  size_t contextSize(const Frame& fr) const override {
    return effect.contextSize(fr);
  }

  void begin(const Frame& fr) const override {
    Frame tfr = fr;
    tfr.origin = transform(fr.origin);
    tfr.size = transform(fr.size);
    effect.begin(tfr);
  }

  void end(const Frame& fr) const override {
    Frame tfr = fr;
    tfr.origin = transform(fr.origin);
    tfr.size = transform(fr.size);
    effect.end(tfr);
  }

  Color color(Point pt, const Frame& fr) const override {
    Frame tfr = fr;
    tfr.origin = transform(fr.origin);
    tfr.size = transform(fr.size);
    return effect.color(transform(pt), tfr);
  }

  Transform transform;
  T effect;
};

template <typename T>
Transformed<T> transform(Transform tr, const T& ef) {
  return Transformed<T>(tr, ef);
}

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_TRANSFORM_H */
