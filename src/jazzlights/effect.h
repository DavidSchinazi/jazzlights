#ifndef JL_EFFECT_H
#define JL_EFFECT_H

#include <string>

#include "jazzlights/frame.h"
#include "jazzlights/util/color.h"
#include "jazzlights/util/geom.h"
#include "jazzlights/util/log.h"

namespace jazzlights {

class Effect {
 public:
  virtual ~Effect() = default;

  // The Player will first call contextSize() before any other calls.
  virtual size_t contextSize(const Frame& frame) const = 0;
  // Then the Player ensures that frame.context can hold that much memory and calls begin().
  virtual void begin(const Frame& frame) const = 0;
  // Then, for each separate point in time to render, the Player calls rewind().
  virtual void rewind(const Frame& frame) const = 0;
  // Then, for each pixel, the player calls color().
  virtual Color color(const Frame& frame, const Pixel& px) const = 0;
  // After the calls to color(), and only once per time period to render, the Player calls afterColors().
  // Every call to rewind() is matched with exactly one call to afterColors().
  virtual void afterColors(const Frame& frame) const = 0;
  virtual std::string effectName(PatternBits pattern) const = 0;
};

template <typename STATE, typename PER_PIXEL_TYPE>
class XYIndexStateEffect : public Effect {
 public:
  virtual void innerBegin(const Frame& frame, STATE* state) const = 0;
  virtual void innerRewind(const Frame& frame, STATE* state) const = 0;
  virtual Color innerColor(const Frame& frame, STATE* state, const Pixel& px) const = 0;

  size_t contextSize(const Frame& frame) const override {
    return sizeof(XYIndex) + sizeof(STATE) + sizeof(PER_PIXEL_TYPE) * width(frame) * height(frame);
  }

  Color color(const Frame& frame, const Pixel& px) const override {
    *pos(frame) = frame.xyIndexStore->FromPixel(px);
    return innerColor(frame, state(frame), px);
  }

  void begin(const Frame& frame) const override {
    new (pos(frame)) XYIndex;                                          // Default-initialize the position.
    new (state(frame)) STATE;                                          // Default-initialize the state.
    new (pixels(frame)) PER_PIXEL_TYPE[width(frame) * height(frame)];  // Default-initialize the per-pixel data.
    innerBegin(frame, state(frame));
  }

  void rewind(const Frame& frame) const override { innerRewind(frame, state(frame)); }

  void afterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<STATE>::value, "STATE must be trivially destructible");
    static_assert(std::is_trivially_destructible<PER_PIXEL_TYPE>::value,
                  "PER_PIXEL_TYPE must be trivially destructible");
  }

 protected:
  size_t x(const Frame& f) const { return pos(f)->xIndex; }
  size_t y(const Frame& f) const { return pos(f)->yIndex; }
  size_t w(const Frame& f) const { return width(f); }
  size_t h(const Frame& f) const { return height(f); }
  PER_PIXEL_TYPE& ps(const Frame& f, size_t x, size_t y) const { return pixels(f)[y * w(f) + x]; }
  PER_PIXEL_TYPE& ps(const Frame& f) const { return ps(f, x(f), y(f)); }
  STATE* state(const Frame& frame) const {
    return reinterpret_cast<STATE*>(reinterpret_cast<uint8_t*>(frame.context) + sizeof(XYIndex));
  }

 private:
  size_t width(const Frame& frame) const { return frame.xyIndexStore->xValuesCount(); }
  size_t height(const Frame& frame) const { return frame.xyIndexStore->yValuesCount(); }
  XYIndex* pos(const Frame& frame) const { return reinterpret_cast<XYIndex*>(frame.context); }
  PER_PIXEL_TYPE* pixels(const Frame& frame) const {
    return reinterpret_cast<PER_PIXEL_TYPE*>(reinterpret_cast<uint8_t*>(frame.context) + sizeof(XYIndex) +
                                             sizeof(STATE));
  }
};

struct EmptyState {};

template <typename PER_PIXEL_TYPE>
class XYIndexEffect : public XYIndexStateEffect<EmptyState, PER_PIXEL_TYPE> {
 public:
  virtual void innerBegin(const Frame& frame) const = 0;
  virtual void innerRewind(const Frame& frame) const = 0;
  virtual Color innerColor(const Frame& frame, const Pixel& px) const = 0;
  void innerBegin(const Frame& frame, EmptyState* /*state*/) const override { innerBegin(frame); }
  void innerRewind(const Frame& frame, EmptyState* /*state*/) const override { innerRewind(frame); }
  Color innerColor(const Frame& frame, EmptyState* /*state*/, const Pixel& px) const override {
    return innerColor(frame, px);
  }
};

}  // namespace jazzlights
#endif  // JL_EFFECT_H
