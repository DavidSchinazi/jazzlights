#ifndef UNISPARKS_EFFECT_WRAPPER_HPP
#define UNISPARKS_EFFECT_WRAPPER_HPP
#include "unisparks/effect.hpp"

namespace unisparks {


template<typename T>
struct WrappedEffect : Effect {
  WrappedEffect(const T& ef) : effect(ef) {
  }

  size_t contextSize(const Animation& am) const override {
    return effect.contextSize(transform(am));
  }

  void begin(const Frame& fr) const override {
    effect.begin(transform(fr));
  }

  void rewind(const Frame& fr) const override {
    effect.rewind(transform(fr));
  }

  Color color(const Pixel& px) const override {
    return effect.color(transform(px));
  }

  void end(const Animation& am) const override {
    effect.end(transform(am));
  }

  template<typename P> P transform(const P& v) const {
    return v;
  }

  T effect;
};



} // namespace unisparks
#endif /* UNISPARKS_EFFECT_WRAPPER_HPP */
