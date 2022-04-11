#ifndef UNISPARKS_EFFECTS_TRANSFORM_H
#define UNISPARKS_EFFECTS_TRANSFORM_H
#include "unisparks/effect.hpp"

namespace unisparks {

template<typename T>
struct Transformed : Effect {
  Transformed(const T& ef) : effect(ef) {
  }

  size_t contextSize(const Animation& am) const override {
    return effect.contextSize(am);
  }

  void begin(const Frame& fr) const override {
    effect.begin(tframe(fr));
  }

  void rewind(const Frame& fr) const override {
    effect.rewind(tframe(fr));
  }

  Color color(const Pixel& px) const override {
    //auto tpx = tpixel(px);
    //info(">>> %f,%f -> %f,%f", px.coord.x, px.coord.y, tpx.coord.x, tpx.coord.y);
    return effect.color(tpixel(px));
  }

  Animation tanim(Animation am) const {
    am.viewport = transform(am.viewport);
    return am;
  }

  Frame tframe(Frame fr) const {
    fr.animation = tanim(fr.animation);
    return fr;
  }

  std::string name() const override {
    return std::string("transform-") + effect.name();
  }

  Pixel tpixel(Pixel px) const {
    px.frame = tframe(px.frame);
    px.coord = transform(px.coord);
    return px;
  }

  Transform transform = IDENTITY;
  T effect;
};

template <typename T>
Transformed<T> transform(Transform tr, const T& ef) {
  Transformed<T> res(ef);
  res.transform = tr;
  return res;
}

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_TRANSFORM_H */
