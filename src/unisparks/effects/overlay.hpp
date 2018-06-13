#ifndef UNISPARKS_EFFECTS_OVERLAY_H
#define UNISPARKS_EFFECTS_OVERLAY_H
#include "unisparks/effect.hpp"

namespace unisparks {

using BlendFunction = Color(*)(Color, Color);

class BasicOverlay : public Effect {
 public:
  BasicOverlay(BlendFunction bf, const Effect* fg = nullptr,
               const Effect* bg = nullptr) : blend_(bf), foreground_(fg), background_(bg) {
  }

  size_t contextSize(const Frame& fr) const override {
    if (!background_) {
      return 0;
    }
    if (!foreground_) {
      return background_->contextSize(fr);
    }
    return background_->contextSize(fr) + foreground_->contextSize(fr);
  }

  void begin(const Frame& fr) const override {
    if (!background_) {
      return;
    }
    background_->begin(fr);

    if (!foreground_) {
      return;
    }

    Frame fgfr = fr;
    fgfr.context = static_cast<char*>(fr.context) + background_->contextSize(fr);
    foreground_->begin(fgfr);
  }

  void end(const Frame& fr) const override {
    if (!background_) {
      return;
    }

    if (foreground_) {
      Frame fgfr = fr;
      fgfr.context = static_cast<char*>(fr.context) + background_->contextSize(fr);
      foreground_->end(fgfr);
    }

    background_->end(fr);
  }

  Color color(Point pt, const Frame& fr) const override {
    if (!background_) {
      return TRANSPARENT;
    }

    Color res = background_->color(pt, fr);

    if (foreground_) {
      Frame fgfr = fr;
      fgfr.context = static_cast<char*>(fr.context) + background_->contextSize(fr);
      res = blend_(foreground_->color(pt, fgfr), res);
    }

    return res;
  }

 protected:
  void reset(Effect* fg, Effect* bg) {
    foreground_ = fg;
    background_ = bg;
  }

 private:
  BlendFunction blend_;
  const Effect* foreground_;
  const Effect* background_;
};

template<typename Foreground, typename Background>
struct Overlay : BasicOverlay {

  Overlay(BlendFunction bf, const Foreground& fg,
          const Background& bg) : BasicOverlay(bf), foreground(fg), background(bg) {
    reset(&foreground, &background);
  }

  Foreground foreground;
  Background background;
};


template <typename F, typename B>
Overlay<F, B> overlay(BlendFunction bf, const F& fg, const B& bg) {
  return Overlay<F, B>(bf, fg, bg);
}


} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_OVERLAY_H */
