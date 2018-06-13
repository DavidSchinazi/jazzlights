#ifndef UNISPARKS_EFFECT_FUNCTIONAL_H
#define UNISPARKS_EFFECT_FUNCTIONAL_H
#include "unisparks/effect.hpp"
#include "unisparks/util/meta.hpp"

namespace unisparks {

template<typename F>
class FunctionalEffect : public Effect {
 public:
  using ContextT = typename function_traits<F>::return_type;

  FunctionalEffect(const F& f) : initFrame_(f) {
  }

  FunctionalEffect(const FunctionalEffect& other) = default;

  size_t contextSize(const Frame&) const override {
    return sizeof(ContextT);
  }

  void begin(const Frame& frame) const override {
    new(frame.context) ContextT(initFrame_(frame));
  }

  void end(const Frame& frame) const override {
    static_cast<ContextT*>(frame.context)->~ContextT();
  }

  Color color(Point pt, const Frame& frame) const override {
    return (*static_cast<ContextT*>(frame.context))(pt);
  }

 private:
  F initFrame_;
};

template<typename F>
constexpr FunctionalEffect<F> effect(const F& f) {
  return FunctionalEffect<F>(f);
}


} // namespace unisparks
#endif /* UNISPARKS_EFFECT_FUNCTIONAL_H */
