#ifndef DFSPARKS_PIXELS_H
#define DFSPARKS_PIXELS_H
#include "dfsparks/color.h"
namespace dfsparks {

class Pixels {
public:
  Pixels() = default;
  virtual ~Pixels() = default;

  int count() const { return getNumberOfPixels(); }
  int left() const { return 0; }
  int top() const { return 0; }
  int width() const { return iabs(tr_[0] * getWidth() + tr_[1] * getHeight()); }
  int height() const {
    return iabs(tr_[2] * getWidth() + tr_[3] * getHeight());
  }
  void coords(int index, int *x, int *y) const {
    int xx, yy;
    getCoords(index, &xx, &yy);
    *x = tr_[0] * xx + tr_[1] * yy;
    *y = tr_[2] * xx + tr_[3] * yy;
  }

  void fill(RgbaColor color) {
    for (int i = 0; i < count(); ++i) {
      setColor(i, color);
    }
  }

  void setColor(int i, RgbaColor color) { doSetColor(i, color); }

  Pixels &rotateLeft() {
    tr_[0] = 0;  // cos(a);
    tr_[1] = 1;  // sin(a);
    tr_[2] = -1; //-sin(a);
    tr_[3] = 0;  // cos(a);
    return *this;
  }

  Pixels &rotateRight() {
    tr_[0] = 0;  // cos(a);
    tr_[1] = -1; // sin(a);
    tr_[2] = 1;  //-sin(a);
    tr_[3] = 0;  // cos(a);
    return *this;
  }

private:
  int iabs(int n) const { return n > 0 ? n : -n; }

  virtual int getNumberOfPixels() const = 0;
  virtual int getWidth() const = 0;
  virtual int getHeight() const = 0;
  virtual void getCoords(int i, int *x, int *y) const = 0;
  virtual void doSetColor(int index, RgbaColor color) = 0;

  int tr_[4] = {1, 0, 0, 1};
};

// class Rotated : public Pixels {
// public:
//   Rotated(Pixels& orig, double a) : p_(orig) {
//     tr_[0] = cos(a);
//     tr_[1] = sin(a);
//     tr_[2] = -sin(a);
//     tr_[3] = cos(a);
//   }

// private:
//   int getNumberOfPixels() const final { return p_.count(); }
//   int getWidth() const final {return tr_[0]*p_.width() + tr_[1]*p_.height();
//   }
//   int getHeight() const final {return tr_[2]*width() + tr_[3]*height(); }
//   void getCoords(int i, int *x, int *y) const final {
//     int xx, yy;
//     p_.coords(i, &xx, &yy);
//     *x = tr_[0]*xx + tr_[1]*yy;
//     *y = tr_[2]*xx + tr_[3]*yy;
//   }
//   void doSetColor(int index, uint8_t red, uint8_t green, uint8_t blue,
//                           uint8_t alpha) final
//                           {p_.setColor(index,rgba(red,green,blue,alpha));}

//   Pixels& p_;
//   int tr_[4] = {1, 0, 0, 1};
// };

class VerticalStrand : public Pixels {
public:
  VerticalStrand(int l, int p = 0) : length_(l), pos_(p) {}
  int length() const { return length_; }

private:
  int length_;
  int pos_;

  int getNumberOfPixels() const final { return length_; };
  int getWidth() const final { return 1; }
  int getHeight() const final { return length_; }
  void getCoords(int i, int *x, int *y) const final {
    *x = pos_;
    *y = i;
  }
};

class HorizontalStrand : public Pixels {
public:
  HorizontalStrand(int l, int p = 0) : length_(l), pos_(p) {}
  int length() const { return length_; }

private:
  int length_;
  int pos_;

  int getNumberOfPixels() const final { return length_; };
  int getWidth() const final { return length_; }
  int getHeight() const final { return 1; }
  void getCoords(int i, int *x, int *y) const final {
    *x = i;
    *y = pos_;
  }
};

class Matrix : public Pixels {
public:
  Matrix(int w, int h) : width_(w), height_(h) {}
  int width() const { return width_; }
  int height() const { return height_; }

private:
  int width_;
  int height_;

  virtual void doSetColor(int x, int y, RgbaColor color) = 0;

  int getNumberOfPixels() const final { return width_ * height_; };
  int getWidth() const final { return width_; }
  int getHeight() const final { return height_; }
  void getCoords(int i, int *x, int *y) const final {
    *x = i % width_;
    *y = i / width_;
  }
  void doSetColor(int index, RgbaColor color) final {
    int x, y;
    getCoords(index, &x, &y);
    doSetColor(x, y, color);
  }
};

class PixelMap : public Pixels {
public:
  PixelMap(int w, int h, int pxcnt, const int *map, bool rotated = false)
      : width_(w), height_(h), numberOfPixels_(pxcnt) {
    xMap_ = new int[numberOfPixels_];
    yMap_ = new int[numberOfPixels_];
    for (int x = 0; x < w; ++x) {
      for (int y = 0; y < h; ++y) {
        int i = map[x + y * w];
        if (i < numberOfPixels_) {
          // UGLY HACK, WILL ONLY WORK FOR THE VEST
          // TODO: Implement proper rotation on pixel level
          if (!rotated) {
            xMap_[i] = x;
            yMap_[i] = y;
          } else {
            yMap_[i] = w - x;
            xMap_[i] = y;
          }
        }
      }
    }
  }

  ~PixelMap() {
    delete[] xMap_;
    delete[] yMap_;
  }

private:
  int getNumberOfPixels() const final { return numberOfPixels_; };
  int getWidth() const { return width_; }
  int getHeight() const { return height_; }
  void getCoords(int i, int *x, int *y) const final {
    *x = xMap_[i];
    *y = yMap_[i];
  }

  int width_;
  int height_;
  int numberOfPixels_;
  int *xMap_;
  int *yMap_;
};

} // namespace dfsparks
#endif /* DFSPARKS_PIXELS_H_ */
