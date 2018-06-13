#ifndef UNISPARKS_LAYOUT_H
#define UNISPARKS_LAYOUT_H
#include "unisparks/util/geom.hpp"
#include "unisparks/util/stream.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

class Layout {
 public:
  //Layout() = default;
  //Layout(const Layout& other) = delete;
  //Layout& operator=(const Layout& other) = delete;
  virtual ~Layout() = default;

  virtual int pixelCount() const = 0;
  virtual Box bounds() const = 0;
  virtual Point at(int i) const = 0;

  class Iterator {
   public:
    using value_type = Point;
    using reference_type = Point&;

    Iterator(const Layout& l, int i) : layout_(&l), index_(i) {
    }

    Point operator*() const {
      return layout_->at(index_);
    }

    bool operator==(const Iterator& other) const {
      return layout_ == other.layout_ && index_ == other.index_;
    }

    bool operator!=(const Iterator& other) {
      return !(*this == other);
    }

    Iterator& operator++() {
      index_++;
      return *this;
    }

    Iterator& operator++(int) {
      index_++;
      return *this;
    }

   private:
    const Layout* layout_;
    int index_;
  };

  Iterator begin() const {
    return Iterator(*this, 0);
  }

  Iterator end() const {
    return Iterator(*this, pixelCount());
  }

  RangeInputStream<Iterator> pixels() const {
    return RangeInputStream<Iterator>(begin(), end());
  }

  Box calculateBounds() {
    Box bounds;
    bounds.origin = at(0);
    Point rb = at(0);
    for(int i=1; i<pixelCount(); ++i) {
      bounds.origin.x = min(bounds.origin.x, at(i).x);
      bounds.origin.y = min(bounds.origin.y, at(i).y);
      rb.x = max(rb.x, at(i).x);
      rb.y = max(rb.y, at(i).y);       
    }
    bounds.size.width = rb.x - bounds.origin.x;
    bounds.size.height = rb.y - bounds.origin.y;
    return bounds;
  }

};

} // namespace unisparks

#endif /* UNISPARKS_LAYOUT_H */
