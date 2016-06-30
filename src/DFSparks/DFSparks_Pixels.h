#ifndef DFSPARKS_PIXELS_H
#define DFSPARKS_PIXELS_H
#include "DFSparks_Color.h"
namespace dfsparks {

class Pixels {
public:
  Pixels() = default;
  virtual ~Pixels() = default;

  int count() const { return on_get_number_of_pixels(); }
  int get_width() const { return on_get_width(); }
  int get_height() const { return on_get_height(); }
  void get_coords(int index, int* x, int* y) const { on_get_coords(index, x, y); }  

  void fill(uint32_t color) {
    for (int i=0; i<count(); ++i) {
      set_color(i, color);
    }
  }

  void set_color(int i, uint32_t color) {
    on_set_pixel_color(i, chan_red(color), chan_green(color),
                       chan_blue(color), chan_alpha(color));
  }

private:
  virtual int on_get_number_of_pixels() const = 0;
  virtual int on_get_width() const = 0;
  virtual int on_get_height() const = 0;
  virtual void on_get_coords(int i, int *x, int *y) const = 0;
  virtual void on_set_pixel_color(int index, uint8_t red, uint8_t green,
                                  uint8_t blue, uint8_t alpha) = 0;

};

} // namespace dfsparks
#endif /* DFSPARKS_PIXELS_H_ */
