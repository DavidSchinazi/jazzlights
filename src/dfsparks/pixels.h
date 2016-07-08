#ifndef DFSPARKS_PIXELS_H
#define DFSPARKS_PIXELS_H
#include "dfsparks/color.h"
namespace dfsparks {

class Pixels {
public:
  Pixels() = default;
  virtual ~Pixels() = default;

  int count() const { return on_get_number_of_pixels(); }
  int get_left() const { return 0; }
  int get_top() const { return 0; }
  int get_width() const { return on_get_width(); }
  int get_height() const { return on_get_height(); }
  void get_coords(int index, int *x, int *y) const {
    on_get_coords(index, x, y);
  }

  void fill(uint32_t color) {
    for (int i = 0; i < count(); ++i) {
      set_color(i, color);
    }
  }

  void set_color(int i, uint32_t color) { on_set_color(i, color); }

private:
  virtual int on_get_number_of_pixels() const = 0;
  virtual int on_get_width() const = 0;
  virtual int on_get_height() const = 0;
  virtual void on_get_coords(int i, int *x, int *y) const = 0;
  virtual void on_set_color(int index, uint32_t color) = 0;
};

class VerticalStrand : public Pixels {
public:
  VerticalStrand(int l, int p = 0) : length(l), pos(p) {}
  int get_length() const { return length; }

private:
  int length;
  int pos;

  int on_get_number_of_pixels() const final { return length; };
  int on_get_width() const final { return 1; }
  int on_get_height() const final { return length; }
  void on_get_coords(int i, int *x, int *y) const final {
    *x = pos;
    *y = i;
  }
};

class HorizontalStrand : public Pixels {
public:
  HorizontalStrand(int l, int p = 0) : length(l), pos(p) {}
  int get_length() const { return length; }

private:
  int length;
  int pos;

  int on_get_number_of_pixels() const final { return length; };
  int on_get_width() const final { return length; }
  int on_get_height() const final { return 1; }
  void on_get_coords(int i, int *x, int *y) const final {
    *x = i;
    *y = pos;
  }
};

class Matrix : public Pixels {
public:
  Matrix(int w, int h) : width(w), height(h) {}
  int get_width() const { return width; }
  int get_height() const { return height; }

private:
  int width;
  int height;

  virtual void on_set_color(int x, int y, uint32_t color) = 0;

  int on_get_number_of_pixels() const final { return width * height; };
  int on_get_width() const final { return width; }
  int on_get_height() const final { return height; }
  void on_get_coords(int i, int *x, int *y) const final {
    *x = i % width;
    *y = i / width;
  }
  void on_set_color(int index, uint32_t color) final {
    int x, y;
    on_get_coords(index, &x, &y);
    on_set_color(x, y, color);
  }
};

} // namespace dfsparks
#endif /* DFSPARKS_PIXELS_H_ */
