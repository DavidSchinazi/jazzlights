#ifndef DFSPARKS_EFFECT_H
#define DFSPARKS_EFFECT_H
#include "DFSparks_Pixels.h"
#include "DFSparks_Timer.h"
#include <stdint.h>

namespace dfsparks {

/**
 * Effect interface
 */
class Effect {
public:
  Effect() noexcept = default;
  virtual ~Effect() noexcept = default;

  void begin(int x, int y, int w, int h, int npix) {
    frame_x = x;
    frame_y = y;
    frame_width = w;
    frame_height = h;
    num_pixels = npix;
    start_time = frame_time = time_ms();
    beat_time = -1;
    on_begin(x,y,w,h,num_pixels);
  }

  void sync(int32_t start_t, int32_t beat_t) {
    start_time = start_t;
    beat_time = beat_t;
  }

  void end() { on_end(); }

  void start_frame() { frame_time = time_ms(); on_start_frame();}
  void finish_frame() {on_finish_frame();}

  void get_frame_bounds(int &x, int &y, int &w, int &h) const { x = frame_x; y = frame_y; w = frame_width; h = frame_height; }
  int get_number_of_pixels() const { return num_pixels; }

  uint32_t get_pixel_color(int x, int y) const {
    return on_get_pixel_color(x,y);
  }
  int32_t get_start_time() const { return start_time; }
  int32_t get_elapsed_time() const { return frame_time - start_time; }
  int32_t get_time_since_beat() const {
    return beat_time < 0 ? INT32_MAX : render_time - beat_time;
  }

private:
  virtual void on_begin(int x, int y, int w, int h, int num_pixels) {}
  virtual void on_end() {}
  virtual void on_start_frame() {}
  virtual void on_finish_frame() {}
  virtual uint32_t on_get_pixel_color(int x, int y) const = 0;

  int frame_x = 0;
  int frame_y = 0;
  int frame_width = 0;
  int frame_height = 0;
  int num_pixels = 0;
  int32_t elapsed_time = -1;
  int32_t start_time = -1;
  int32_t beat_time = -1;
};


/**
 * Effect interface
 */
class BufferedEffect : public Effect {
public:

  void begin(Matrix &pixels) {
    start_time = render_time = time_ms();
    beat_time = -1;
    on_begin(pixels);
  }

  void sync(int32_t start_t, int32_t beat_t) {
    start_time = start_t;
    beat_time = beat_t;
  }

  void end(Matrix &pixels) { on_end(pixels); }

  void render(Matrix &pixels) {
    render_time = time_ms();
    on_render(pixels);
  }

  int32_t get_start_time() const { return start_time; }

  int32_t get_elapsed_time() const { return render_time - start_time; }

  int32_t get_time_since_beat() const {
    return beat_time < 0 ? INT32_MAX : render_time - beat_time;
  }

private:
  virtual void on_begin(Matrix &pixels) {}
  virtual void on_end(Matrix &pixels) {}
  virtual void on_render(Matrix &pixels) = 0;

  int32_t render_time = -1;
  int32_t start_time = -1;
  int32_t beat_time = -1;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_H */