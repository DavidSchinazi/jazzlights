#ifndef UNISPARKS_EFFECT_FUNCTIONAL_H
#define UNISPARKS_EFFECT_FUNCTIONAL_H
#include "unisparks/effect.hpp"
#include "unisparks/util/meta.hpp"

namespace unisparks {

template<typename F>
class FrameFuncEffect : public Effect {
 public:
  using ContextT = typename function_traits<F>::return_type;

  FrameFuncEffect(const std::string& name, const F& f) :
    name_(name), initFrame_(f) {}

  FrameFuncEffect(const FrameFuncEffect& other) = default;

  size_t contextSize(const Animation&) const override {
    return sizeof(ContextT);
  }

  void begin(const Frame& frame) const override {
    new(frame.animation.context) ContextT(initFrame_(frame));
  }

  void rewind(const Frame& frame) const override {
    static_cast<ContextT*>(frame.animation.context)->~ContextT();
    new(frame.animation.context) ContextT(initFrame_(frame));
  }

  Color color(const Pixel& pixel) const override {
    return (*static_cast<ContextT*>(pixel.frame.animation.context))(pixel);
  }

  std::string effectName(PatternBits /*pattern*/) const override {
    return name_;
  }

 private:
  std::string name_;
  F initFrame_;
};


template<typename F>
constexpr FrameFuncEffect<F> effect(const std::string& name, const F& f) {
  return FrameFuncEffect<F>(name, f);
}


} // namespace unisparks
#endif /* UNISPARKS_EFFECT_FUNCTIONAL_H */
