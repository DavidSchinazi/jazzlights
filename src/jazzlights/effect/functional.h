#ifndef JL_EFFECT_FUNCTIONAL_H
#define JL_EFFECT_FUNCTIONAL_H

#include <functional>

#include "jazzlights/effect/effect.h"

namespace jazzlights {

using PixelColorFunc = std::function<CRGB(const Pixel&)>;
using FrameToPixelColorFuncFunc = std::function<PixelColorFunc(const Frame&)>;
// The FunctionalEffect class takes as input a FrameToPixelColorFuncFunc.
// For every time period, it calls its FrameToPixelColorFuncFunc with the frame to get a PixelColorFunc.
// The PixelColorFunc is saved in the frame context, and then it is called for every pixel.
class FunctionalEffect : public Effect {
 public:
  explicit FunctionalEffect(const std::string& name, const FrameToPixelColorFuncFunc& f) : name_(name), frameFunc_(f) {}

  // Disallow copy constructor and assignment.
  FunctionalEffect(const FunctionalEffect&) = delete;
  FunctionalEffect& operator=(const FunctionalEffect&) = delete;
  FunctionalEffect& operator=(FunctionalEffect&&) = delete;
  // But allow move constructor.
  FunctionalEffect(FunctionalEffect&&) = default;

  size_t contextSize(const Frame& /*frame*/) const override { return sizeof(PixelColorFunc); }

  void begin(const Frame& /*frame*/) const override {}

  void rewind(const Frame& frame) const override {
    // Note that this call to new does not allocate heap memory.
    // It calls frameFunc_(frame) and places the result in the frame context.
    new (GetPixelColorFuncMemory(frame)) PixelColorFunc(frameFunc_(frame));
  }

  void afterColors(const Frame& frame) const override {
    // Call the destructor for the PixelColorFunc currently saved in the frame context.
    GetPixelColorFuncMemory(frame)->~PixelColorFunc();
  }

  CRGB color(const Frame& frame, const Pixel& px) const override { return (*GetPixelColorFuncMemory(frame))(px); }

  std::string effectName(PatternBits /*pattern*/) const override { return name_; }

 private:
  PixelColorFunc* GetPixelColorFuncMemory(const Frame& frame) const {
    static_assert(alignof(PixelColorFunc) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<PixelColorFunc*>(frame.context);
  }
  const std::string name_;
  const FrameToPixelColorFuncFunc frameFunc_;
};

inline FunctionalEffect effect(const std::string& name, const FrameToPixelColorFuncFunc& f) {
  return FunctionalEffect(name, f);
}

}  // namespace jazzlights
#endif  // JL_EFFECT_FUNCTIONAL_H
