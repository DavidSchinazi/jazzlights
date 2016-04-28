#ifndef DFSPARKS_PIXELS_H
#define DFSPARKS_PIXELS_H

namespace dfsparks {

template <int mwidth, int mheight, typename Renderer> class Pixels2D {
public:
  static const int width = mwidth;
  static const int height = mheight;

  Pixels2D(Renderer &f) : set_pixel_color(f) {}

  void fill(uint32_t color) {
    for (int x = width; x >= 0; --x) {
      for (int y = height; y >= 0; --y) {
        set_pixel_color(x, y, color);
      }
    }
  }

  void set_color(int x, int y, uint32_t color) { set_pixel_color(x, y, color); }

private:
  Renderer &set_pixel_color;
};

// template<typename Pixels>
// struct RotatedLeft : public Pixels {
// 	const int width = Pixels::height;
// 	const int height = Pixels::width;

// 	void set_color(int x, int y, uint8_t color) {
// 		render(y, x, chan_red(color), chan_green(color),
// chan_blue(color));
// 	}
// };

} // namespace dfsparks
#endif /* DFSPARKS_PIXELS_H_ */
