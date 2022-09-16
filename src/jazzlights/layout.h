#ifndef JAZZLIGHTS_LAYOUT_H
#define JAZZLIGHTS_LAYOUT_H

#include "jazzlights/types.h"
#include "jazzlights/util/geom.h"
#include "jazzlights/util/stream.h"
#include "jazzlights/util/math.h"

namespace jazzlights {

using PixelsPerMeter = double;

class Layout {
 public:
  Layout() = default;
  Layout(const Layout& other) = delete;
  virtual ~Layout() = default;
  Layout& operator=(const Layout& other) = delete;

  virtual int pixelCount() const = 0;
  virtual Point at(int i) const = 0;
};

class LayoutIterator {
 public:
  using value_type = Pixel;
  using reference_type = Pixel&;

  LayoutIterator(const Layout& l, int i) : layout_(&l), index_(i) {
  }

  Pixel operator*() const {
    Pixel px;
    px.layout = layout_;
    px.index = index_;
    px.coord = layout_->at(index_);
    return px;
  }

  bool operator==(const LayoutIterator& other) const {
    return layout_ == other.layout_ && index_ == other.index_;
  }

  bool operator!=(const LayoutIterator& other) {
    return !(*this == other);
  }

  size_t operator-(const LayoutIterator& other) const {
    return index_ - other.index_;
  }

  LayoutIterator& operator++() {
    index_++;
    return *this;
  }

  LayoutIterator& operator++(int) {
    index_++;
    return *this;
  }

 private:
  const Layout* layout_;
  int index_;
};


inline LayoutIterator begin(const Layout& l) {
  return LayoutIterator(l, 0);
}

inline LayoutIterator end(const Layout& l) {
  return LayoutIterator(l, l.pixelCount());
}

inline RangeInputStream<LayoutIterator> points(const Layout& l) {
  return RangeInputStream<LayoutIterator>(begin(l), end(l));
}

inline Box bounds(const Layout& layout) {
  Box bb;
  for (auto pt : layout) {
      bb = merge(bb, pt.coord);
  }
  return bb;
}

}  // namespace jazzlights

#endif  // JAZZLIGHTS_LAYOUT_H
