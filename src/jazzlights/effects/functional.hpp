#ifndef JAZZLIGHTS_EFFECT_FUNCTIONAL_H
#define JAZZLIGHTS_EFFECT_FUNCTIONAL_H
#include "jazzlights/effect.hpp"
#include "jazzlights/util/meta.hpp"

namespace jazzlights {

template<typename F>
class FrameFuncEffect : public Effect {
 public:
  using ContextT = typename function_traits<F>::return_type;

  FrameFuncEffect(const std::string& name, const F& f) :
    name_(name), initFrame_(f) {}

  FrameFuncEffect(const FrameFuncEffect& other) = default;

  size_t contextSize(const Frame& /*frame*/) const override {
    return sizeof(ContextT);
  }

  void begin(const Frame& frame) const override {
    new(frame.context) ContextT(initFrame_(frame));
  }

  void rewind(const Frame& frame) const override {
    static_cast<ContextT*>(frame.context)->~ContextT();
    new(frame.context) ContextT(initFrame_(frame));
  }

  Color color(const Frame& frame, const Pixel& px) const override {
    return (*static_cast<ContextT*>(frame.context))(px);
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


}  // namespace jazzlights
#endif  // JAZZLIGHTS_EFFECT_FUNCTIONAL_H
