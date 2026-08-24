#ifndef JL_EFFECT_EFFECT_H
#define JL_EFFECT_EFFECT_H

#include <string>

#include "jazzlights/protocol/wire_types.h"
#include "jazzlights/render/fastled_wrapper.h"
#include "jazzlights/render/frame.h"
#include "jazzlights/render/pixel.h"
#include "jazzlights/render/predictable_random.h"
#include "jazzlights/util/geom.h"
#include "jazzlights/util/log.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

inline constexpr FrameTimeMs kEffectDurationMs =
    static_cast<FrameTimeMs>(kEffectDuration / kMicrosecondsPerMillisecond);

inline constexpr size_t kMaxStateAlignment = 8;
static_assert((kMaxStateAlignment & (kMaxStateAlignment - 1)) == 0, "kMaxStateAlignment must be a power of 2");
static_assert(kMaxStateAlignment >= sizeof(void*), "kMaxStateAlignment must be bigger than pointer");

class Effect {
 public:
  virtual ~Effect() = default;

  // The Player will first call ContextSize() before any other calls.
  virtual size_t ContextSize(const Frame& frame) const = 0;
  // Then the Player ensures that frame.context can hold that much memory and calls begin().
  virtual void Begin(const Frame& frame) const = 0;
  // Then, for each separate point in time to render, the Player calls Rewind().
  virtual void Rewind(const Frame& frame) const = 0;
  // Then, for each pixel, the player calls Color().
  virtual CRGB Color(const Frame& frame, const Pixel& px) const = 0;
  // After the calls to Color(), and only once per time period to render, the Player calls AfterColors().
  // Every call to Rewind() is matched with exactly one call to AfterColors().
  virtual void AfterColors(const Frame& frame) const = 0;
  virtual std::string EffectName(PatternBits pattern) const = 0;
};

template <typename STATE, typename PER_PIXEL_TYPE>
class XYIndexStateEffect : public Effect {
 public:
  virtual void InnerBegin(const Frame& frame, STATE* state) const = 0;
  virtual void InnerRewind(const Frame& frame, STATE* state) const = 0;
  virtual CRGB InnerColor(const Frame& frame, STATE* state, const Pixel& px) const = 0;

  size_t ContextSize(const Frame& frame) const override {
    return offsetof(XYIndexState, pixels) + sizeof(PER_PIXEL_TYPE) * Width(frame) * Height(frame);
  }

  CRGB Color(const Frame& frame, const Pixel& px) const override {
    *Pos(frame) = frame.xyIndexStore->FromPixel(px);
    return InnerColor(frame, State(frame), px);
  }

  void Begin(const Frame& frame) const override {
    new (XyIndexState(frame)) XYIndexState;                            // Default-initialize the position and state.
    new (Pixels(frame)) PER_PIXEL_TYPE[Width(frame) * Height(frame)];  // Default-initialize the per-pixel data.
    InnerBegin(frame, State(frame));
  }

  void Rewind(const Frame& frame) const override { InnerRewind(frame, State(frame)); }

  void AfterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<STATE>::value, "STATE must be trivially destructible");
    static_assert(std::is_trivially_destructible<PER_PIXEL_TYPE>::value,
                  "PER_PIXEL_TYPE must be trivially destructible");
  }

 protected:
  size_t X(const Frame& f) const { return Pos(f)->xIndex; }
  size_t Y(const Frame& f) const { return Pos(f)->yIndex; }
  size_t W(const Frame& f) const { return Width(f); }
  size_t H(const Frame& f) const { return Height(f); }
  PER_PIXEL_TYPE& Ps(const Frame& f, size_t x, size_t y) const {
#if JL_BOUNDS_CHECKS
    if (x >= W(f) || y >= H(f)) {
      jll_fatal("ATTEMPTING TO ACCESS BAD MEMORY x=%zu w=%zu y=%zu h=%zu", x, W(f), y, H(f));
    }
#endif  // JL_BOUNDS_CHECKS
    return Pixels(f)[y * W(f) + x];
  }
  PER_PIXEL_TYPE& Ps(const Frame& f) const { return Ps(f, X(f), Y(f)); }
  STATE* State(const Frame& frame) const { return &XyIndexState(frame)->state; }

 private:
  struct XYIndexState {
    XYIndex pos;
    STATE state;
    PER_PIXEL_TYPE pixels[];
  };
  XYIndexState* XyIndexState(const Frame& frame) const {
    static_assert(alignof(XYIndexState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<XYIndexState*>(frame.context);
  }
  size_t Width(const Frame& frame) const { return frame.xyIndexStore->xValuesCount(); }
  size_t Height(const Frame& frame) const { return frame.xyIndexStore->yValuesCount(); }
  XYIndex* Pos(const Frame& frame) const { return &(XyIndexState(frame)->pos); }
  PER_PIXEL_TYPE* Pixels(const Frame& frame) const { return XyIndexState(frame)->pixels; }
};

struct EmptyState {};

template <typename PER_PIXEL_TYPE>
class XYIndexEffect : public XYIndexStateEffect<EmptyState, PER_PIXEL_TYPE> {
 public:
  virtual void InnerBegin(const Frame& frame) const = 0;
  virtual void InnerRewind(const Frame& frame) const = 0;
  virtual CRGB InnerColor(const Frame& frame, const Pixel& px) const = 0;
  void InnerBegin(const Frame& frame, EmptyState* /*state*/) const override { InnerBegin(frame); }
  void InnerRewind(const Frame& frame, EmptyState* /*state*/) const override { InnerRewind(frame); }
  CRGB InnerColor(const Frame& frame, EmptyState* /*state*/, const Pixel& px) const override {
    return InnerColor(frame, px);
  }
};

inline uint8_t FadeSubColor(uint8_t channel, uint8_t intensity) {
  if (intensity == 255) { return channel; }
  if (channel == 255) { return intensity; }
  if (channel == 0 || intensity == 0) { return 0; }
  return static_cast<uint8_t>(static_cast<double>(channel) * static_cast<double>(intensity) / 255.0);
}

inline CRGB FadeColor(CRGB color, uint8_t intensity) {
  return CRGB(FadeSubColor(color.red, intensity), FadeSubColor(color.green, intensity),
              FadeSubColor(color.blue, intensity));
}

}  // namespace jazzlights
#endif  // JL_EFFECT_EFFECT_H
