#ifndef DFSPARKS_PIXELS_H
#define DFSPARKS_PIXELS_H
#include "dfsparks/color.h"
namespace dfsparks {

struct Point {
  int x;
  int y;
};

struct Box {
  int left;
  int top;
  int right;
  int bottom;
};

struct Transform {  
  Point operator()(const Point& point) { return {
    matrix[0] * point.x + matrix[1] * point.y, 
    matrix[2] * point.x + matrix[3] * point.y};
  }
  int matrix[4];
};

static const Transform IDENTITY = {.matrix={1,0,0,1}};
static const Transform ROTATE_LEFT = {.matrix={0,1,-1,0}};
static const Transform ROTATE_RIGHT = {.matrix={0,-1,1,0}};

class Pixels {
public:
  Pixels() = default;
  virtual ~Pixels() = default;

  int count() const {return count_(); }
  void setColor(int index, RgbaColor color) {setColor_(index, color);}
  Point coords(int index) const {return coords_(index);}
  Box boundingBox() const {return boundingBox_(); }

  int width() const {Box bb = boundingBox(); return bb.right - bb.left;}
  int height() const {Box bb = boundingBox(); return bb.bottom - bb.top;}

  void fill(RgbaColor color) {
    for (int i = 0; i < count(); ++i) {
      setColor(i, color);
    }
  }

private:
  virtual int count_() const = 0;
  virtual void setColor_(int index, RgbaColor color) = 0;
  virtual Point coords_(int index) const = 0;
  virtual Box boundingBox_() const  = 0;
};

typedef void (*PixelRenderer)(int, RgbaColor, void*);

class PixelMatrix : public Pixels {
public:
  PixelMatrix(int w, int h, PixelRenderer render, void *context = 0) :
    width_(w), height_(h), render_(render), context_(context) {}

private:
  int width_;
  int height_;
  PixelRenderer render_;
  void *context_;

  int count_() const final { return width_ * height_; }
  void setColor_(int index, RgbaColor color) { render_(index, color, context_); }
  Point coords_(int index) const { return {index % width_, index/width_}; }
  Box boundingBox_() const { return {0,0, width_, height_}; }
};


class PixelMap : public Pixels {
public:
  PixelMap(int w, int h, int pxcnt, PixelRenderer render, const int *map, bool rotated = false, void *context = 0)
      : numberOfPixels(pxcnt), width(w), height(h), render(render), context(context) {
    points = new Point[numberOfPixels];
    for (int x = 0; x < w; ++x) {
      for (int y = 0; y < h; ++y) {
        int i = map[x + y * w];
        if (i < numberOfPixels) {
          // UGLY HACK, WILL ONLY WORK FOR THE VEST
          // TODO: Implement proper rotation on pixel level
          if (!rotated) {
            points[i] = {x,y};
          } else {
            points[i] = {w - x, y};
          }
        }
      }
    }
  }

  ~PixelMap() {
    delete[] points;
  }

private:
  int count_() const final { return numberOfPixels; }
  void setColor_(int index, RgbaColor color) { render(index, color, context); }
  Point coords_(int index) const {return points[index];}
  Box boundingBox_() const {return {0,0, width, height};}

  int numberOfPixels;
  int width;
  int height;
  PixelRenderer render;
  void *context;
  Point *points;
};

} // namespace dfsparks
#endif /* DFSPARKS_PIXELS_H_ */
