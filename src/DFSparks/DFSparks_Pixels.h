#ifndef DFSPARKS_PIXELS_H
#define DFSPARKS_PIXELS_H
#include "DFSparks_Color.h"
namespace dfsparks {

class Matrix {
public:
  Matrix() = default;
  virtual ~Matrix() = default;

  int get_width() const { return on_get_width(); }
  int get_height() const { return on_get_height(); }

  void set_color(int x, int y, uint32_t color) {
    on_set_pixel_color(x, y, chan_red(color), chan_green(color),
                       chan_blue(color), chan_alpha(color));
  }

  void fill(uint32_t color) {
    for (int x = get_width(); x >= 0; --x) {
      for (int y = get_height(); y >= 0; --y) {
        set_color(x, y, color);
      }
    }
  }

private:
  virtual int on_get_width() const = 0;
  virtual int on_get_height() const = 0;
  virtual void on_set_pixel_color(int x, int y, uint8_t red, uint8_t green,
                                  uint8_t blue, uint8_t alpha) = 0;
};

} // namespace dfsparks
#endif /* DFSPARKS_PIXELS_H_ */
