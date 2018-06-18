#ifndef UNISPARKS_EFFECTS_OVERLAY_H
#define UNISPARKS_EFFECTS_OVERLAY_H
#include "unisparks/effect.hpp"

namespace unisparks {

using BlendFunction = Color(*)(Color, Color);

template<typename Foreground, typename Background>
struct Overlay : public Effect {

  Overlay(BlendFunction bf, const Foreground& fg,
          const Background& bg) : blend(bf), foreground(fg), background(bg) {
  }

  size_t contextSize(const Animation& a) const override {
    return background.contextSize(a) + foreground.contextSize(a);
  }

  void begin(const Frame& frame) const override {
    background.begin(frame);
    foreground.begin(fgFrame(frame));
  }

  void rewind(const Frame& f) const override {
    background.rewind(f);
    foreground.rewind(fgFrame(f));
  }

  Color color(const Pixel& px) const override {
    return blend(foreground.color(fgPixel(px)), background.color(px));
  }

  void end(const Animation& a) const override {
    foreground.end(fgAnim(a));
    background.end(a);
  }

  Animation fgAnim(Animation am) const {
    am.context = static_cast<char*>(am.context) + background.contextSize(am);
    return am;
  }

  Frame fgFrame(Frame fr) const {
    fr.animation = fgAnim(fr.animation);
    return fr;
  }

  Pixel fgPixel(Pixel px) const {
    px.frame = fgFrame(px.frame);
    return px;
  }

  BlendFunction blend;
  Foreground foreground;
  Background background;
};

template <typename F, typename B>
Overlay<F, B> overlay(BlendFunction bf, const F& fg, const B& bg) {
  return Overlay<F, B>(bf, fg, bg);
}


} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_OVERLAY_H */
