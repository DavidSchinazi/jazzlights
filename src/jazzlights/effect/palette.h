#ifndef JL_EFFECT_PALETTE_H
#define JL_EFFECT_PALETTE_H

#include <cstdint>

#include "jazzlights/effect/effect.h"
#include "jazzlights/render/fastled_wrapper.h"

namespace jazzlights {

#define ALL_COLORS \
  X(cloud)         \
  X(lava)          \
  X(ocean)         \
  X(forest)        \
  X(rainbow)       \
  X(party)         \
  X(heat)

enum OurColorPalette {
#define X(c) OCP##c,
  ALL_COLORS
#undef X
};

static inline OurColorPalette PaletteFromPattern(PatternBits pattern) {
  constexpr uint8_t firstPaletteBit = 17;
#if 0
  jll_info("PATTERN BITS %u%u%u",
       PatternBit(pattern, firstPaletteBit + 0),
       PatternBit(pattern, firstPaletteBit + 1),
       PatternBit(pattern, firstPaletteBit + 2));
#endif
  if (PatternBit(pattern, firstPaletteBit)) {        // nature
    if (PatternBit(pattern, firstPaletteBit + 1)) {  // rainbow
      return OCPrainbow;
    } else {                                           // frolick
      if (PatternBit(pattern, firstPaletteBit + 2)) {  // forest
        return OCPforest;
      } else {  // party
        return OCPparty;
      }
    }
  } else {                                             // hot&cold
    if (PatternBit(pattern, firstPaletteBit + 1)) {    // cold
      if (PatternBit(pattern, firstPaletteBit + 2)) {  // cloud
        return OCPcloud;
      } else {  // ocean
        return OCPocean;
      }
    } else {                                           // hot
      if (PatternBit(pattern, firstPaletteBit + 2)) {  // lava
        return OCPlava;
      } else {  // heat
        return OCPheat;
      }
    }
  }
}

static inline const TProgmemRGBPalette16* FastLEDPaletteFromOurColorPalette(OurColorPalette ocp) {
  switch (ocp) {
    case OCPcloud: return &CloudColors_p;
    case OCPlava: return &LavaColors_p;
    case OCPocean: return &OceanColors_p;
    case OCPforest: return &ForestColors_p;
    case OCPrainbow: return &RainbowColors_p;
    case OCPparty: return &PartyColors_p;
    case OCPheat: return &HeatColors_p;
  }
  return FastLEDPaletteFromOurColorPalette(OCPrainbow);
}

static inline CRGB colorFromOurPalette(OurColorPalette ocp, uint8_t color) {
  return ColorFromPalette(*FastLEDPaletteFromOurColorPalette(ocp), color);
}

class ColorWithPalette {
 public:
  ColorWithPalette(uint8_t innerColor) : colorOverridden_(false), innerColor_(innerColor) {}
  static ColorWithPalette OverrideColor(CRGB overrideColor) {
    ColorWithPalette col = ColorWithPalette();
    col.overrideColor_ = overrideColor;
    return col;
  }
  static ColorWithPalette OverrideCRGB(CRGB overrideColor) { return OverrideColor(overrideColor); }
  CRGB colorFromPalette(OurColorPalette ocp) const {
    if (colorOverridden_) { return overrideColor_; }
    return colorFromOurPalette(ocp, innerColor_);
  }

 private:
  explicit ColorWithPalette() : colorOverridden_(true) {}
  bool colorOverridden_;
  uint8_t innerColor_;
  CRGB overrideColor_;
};

inline std::string OurColorPaletteName(uint8_t forcedPalette) {
  static const char* kPaletteNames[] = {
      "heat", "lava", "ocean", "cloud", "party", "forest", "rainbow", "rainbow",
  };
  if (forcedPalette < 8) { return kPaletteNames[forcedPalette]; }
  return "unknown";
}

inline std::string PaletteNameFromPattern(PatternBits pattern) {
  switch (PaletteFromPattern(pattern)) {
#define X(c) \
  case OCP##c: return #c;
    ALL_COLORS
#undef X
  }
  return "unknown";
}

template <typename STATE>
class EffectWithPaletteAndState : public Effect {
 public:
  virtual std::string EffectNamePrefix(PatternBits pattern) const = 0;
  virtual size_t ExtraContextSize(const Frame& frame) const {
    (void)frame;
    return 0;
  }
  virtual ColorWithPalette InnerColor(const Frame& frame, const Pixel& px, STATE* state) const = 0;
  virtual void InnerBegin(const Frame& frame, STATE* state) const {
    (void)frame;
    (void)state;
  }
  virtual void InnerRewind(const Frame& frame, STATE* state) const {
    (void)frame;
    (void)state;
  }

  std::string EffectName(PatternBits pattern) const override {
    return EffectNamePrefix(pattern) + "-" + PaletteNameFromPattern(pattern);
  }

  CRGB Color(const Frame& frame, const Pixel& px) const override {
    const ColorWithPalette colorWithPalette = InnerColor(frame, px, &State(frame)->innerState);
    return colorWithPalette.colorFromPalette(State(frame)->ocp);
  }

  size_t ContextSize(const Frame& frame) const override {
    return sizeof(EffectWithPaletteState) + ExtraContextSize(frame);
  }

  void Begin(const Frame& frame) const override {
    new (State(frame)) EffectWithPaletteState;  // Default-initialize the state.
    State(frame)->ocp = PaletteFromPattern(frame.pattern);
    InnerBegin(frame, &State(frame)->innerState);
  }

  void Rewind(const Frame& frame) const override { InnerRewind(frame, &State(frame)->innerState); }

  void AfterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<STATE>::value, "STATE must be trivially destructible");
  }

 protected:
  OurColorPalette palette(const Frame& frame) const { return State(frame)->ocp; }

 private:
  struct EffectWithPaletteState {
    OurColorPalette ocp;
    STATE innerState;
  };
  EffectWithPaletteState* State(const Frame& frame) const {
    static_assert(alignof(EffectWithPaletteState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<EffectWithPaletteState*>(frame.context);
  }
};

using EffectWithPalette = EffectWithPaletteAndState<EmptyState>;

template <typename STATE>
struct EffectWithPaletteState {
  OurColorPalette ocp;
  STATE innerState;
};

template <typename STATE, typename PER_PIXEL_TYPE>
class EffectWithPaletteXYIndexAndState : public XYIndexStateEffect<EffectWithPaletteState<STATE>, PER_PIXEL_TYPE> {
 protected:
  CRGB colorFromPalette(const Frame& frame, uint8_t innerColor) const {
    EffectWithPaletteState<STATE>* s = XYIndexStateEffect<EffectWithPaletteState<STATE>, PER_PIXEL_TYPE>::State(frame);
    return colorFromOurPalette(s->ocp, innerColor);
  }
  OurColorPalette palette(const Frame& frame) const {
    EffectWithPaletteState<STATE>* s = XYIndexStateEffect<EffectWithPaletteState<STATE>, PER_PIXEL_TYPE>::State(frame);
    return s->ocp;
  }

 public:
  virtual std::string EffectNamePrefix(PatternBits pattern) const = 0;
  virtual ColorWithPalette InnerColor(const Frame& frame, STATE* state, const Pixel& px) const = 0;
  virtual void InnerBegin(const Frame& frame, STATE* state) const = 0;
  virtual void InnerRewind(const Frame& frame, STATE* state) const = 0;

  std::string EffectName(PatternBits pattern) const override {
    return EffectNamePrefix(pattern) + "-" + PaletteNameFromPattern(pattern);
  }

  CRGB InnerColor(const Frame& frame, EffectWithPaletteState<STATE>* state, const Pixel& px) const override {
    const ColorWithPalette colorWithPalette = InnerColor(frame, &state->innerState, px);
    return colorWithPalette.colorFromPalette(state->ocp);
  }

  void InnerBegin(const Frame& frame, EffectWithPaletteState<STATE>* state) const override {
    state->ocp = PaletteFromPattern(frame.pattern);
    InnerBegin(frame, &state->innerState);
  }

  void InnerRewind(const Frame& frame, EffectWithPaletteState<STATE>* state) const override {
    InnerRewind(frame, &state->innerState);
  }
};

}  // namespace jazzlights

#endif  // JL_EFFECT_PALETTE_H
