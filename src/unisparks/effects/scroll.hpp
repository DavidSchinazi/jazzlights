#ifndef UNISPARKS_EFFECTS_SCROLL_H
#define UNISPARKS_EFFECTS_SCROLL_H
#include "unisparks/effect.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

template<typename T>
struct Scroll : Effect {

  Scroll(const Point s, const T& ef) : speed(s), effect(ef) {
  }

  size_t contextSize(const Frame& fr) const override {
    return effect.contextSize(fr);
  }

  void begin(const Frame& fr) const override {
    effect.begin(tframe(fr));
  }

  void end(const Frame& fr) const override {
    effect.end(tframe(fr));
  }

  Color color(const Pixel& px) const override {
    Transform tr = IDENTITY;
    tr.offset = (-px.frame.time / 100.0) * speed;
    Pixel tpx = px;
    tpx.coord = tr(px.coord);
    tpx.frame = tframe(px.frame);
    return effect.color(tpx);
  }

  Frame tframe(Frame fr) const {
    Transform tr = IDENTITY;
    tr.offset = (-fr.time / 100.0) * speed;
    fr.animation.viewport.origin = tr(fr.animation.viewport.origin);
    return fr;
  }

  Point speed;
  T effect;
};

template <typename T>
Scroll<T> scroll(Point s, const T& ef) {
  return Scroll<T>(s, ef);
}

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_SCROLL_H */
