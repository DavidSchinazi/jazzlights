#ifndef UNISPARKS_LAYOUTS_TRANSFORMED_H
#define UNISPARKS_LAYOUTS_TRANSFORMED_H
#include "unisparks/layout.hpp"

namespace unisparks {

template<typename L>
class Transformed : public Layout {
 public:
  Transformed(const Transform& t, const L& l) : trans_(t), orig_(l),
    bounds_(calculateBounds()) {
  }

  int pixelCount() const override {
    return orig_.pixelCount();
  }

  Box bounds() const override {
    return bounds_;
  }

  Point at(int i) const override {
    return trans_(orig_.at(i));
  }

 private:
  Transform trans_;
  const L orig_;
  Box bounds_;
};

template<typename L>
Transformed<L> transform(Transform tr, L lo) {
  return Transformed<L> {tr, lo};
}

} // namespace unisparks
#endif /* UNISPARKS_LAYOUTS_TRANSFORMED_H */
