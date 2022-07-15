#ifndef UNISPARKS_EFFECTS_PLASMA_H
#define UNISPARKS_EFFECTS_PLASMA_H
#include "unisparks/config.h"
#include "unisparks/effects/functional.hpp"
#include "unisparks/palette.h"
#include "unisparks/player.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

// SpinPlasma orginally inspired from RGBShadesAudio's audioPlasma.
// https://github.com/macetech/RGBShadesAudio/blob/master/effects.h
class SpinPlasma : public Effect {
public:
  SpinPlasma() = default;

  std::string effectName(PatternBits pattern) const override {
    switch(PaletteFromPattern(pattern)) {
#define X(c) case OCP##c: return "sp-" #c;
  ALL_COLORS
#undef X
    }
    return "sp-unknown";
  }
  Color color(const Frame& frame, const Pixel& px) const override {
    SpinPlasmaState* state = reinterpret_cast<SpinPlasmaState*>(frame.context);
    const uint8_t color =
      internal::sin8(sqrt(square((static_cast<float>(px.coord.x) - state->plasmaCenterX) * state->xMultiplier) +
                          square((static_cast<float>(px.coord.y) - state->plasmaCenterY) * state->yMultiplier)));
    return colorFromOurPalette(state->ocp, color);
  }

  size_t contextSize(const Frame& /*frame*/) const override { return sizeof(SpinPlasmaState); }

  void begin(const Frame& frame) const override {
    SpinPlasmaState* state = reinterpret_cast<SpinPlasmaState*>(frame.context);
    state->ocp = PaletteFromPattern(frame.pattern);
    const float multiplier = frame.predictableRandom->GetRandomNumberBetween(100, 500);
    state->xMultiplier = multiplier / frame.viewport.size.width;
    state->yMultiplier = multiplier / frame.viewport.size.height;
    state->rotationCenterX =
      frame.viewport.origin.x +
      frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.width);
    state->rotationCenterY =
      frame.viewport.origin.y +
      frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.height);
  }

  void rewind(const Frame& frame) const override {
    SpinPlasmaState* state = reinterpret_cast<SpinPlasmaState*>(frame.context);
    const uint8_t offset = 30 * frame.time / 255;
    state->plasmaCenterX = state->rotationCenterX + (static_cast<float>(internal::cos8(offset)) - 127.0) / (state->xMultiplier * 2.0);
    state->plasmaCenterY = state->rotationCenterY + (static_cast<float>(internal::sin8(offset)) - 127.0) / (state->yMultiplier * 2.0);
  }

 private:
  struct SpinPlasmaState {
    OurColorPalette ocp;
    float plasmaCenterX;
    float plasmaCenterY;
    float xMultiplier;
    float yMultiplier;
    float rotationCenterX;
    float rotationCenterY;
  };
  static OurColorPalette PaletteFromPattern(PatternBits pattern) {
    if (patternbit(pattern, 2)) { // nature
      if (patternbit(pattern, 3)) { // rainbow
        return OCPrainbow;
      } else { // frolick
        if (patternbit(pattern, 4)) { // forest
          return OCPforest;
        } else { // party
          return OCPparty;
        }
      }
    } else { // hot&cold
      if (patternbit(pattern, 3)) { // cold
        if (patternbit(pattern, 4)) { // cloud
          return OCPcloud;
        } else { // ocean
          return OCPocean;
        }
      } else { // hot
        if (patternbit(pattern, 4)) { // lava
          return OCPlava;
        } else { // heat
          return OCPheat;
        }
      }
    }
  }
};

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_PLASMA_H */
