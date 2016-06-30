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

  void begin(const Pixels& pixels) {
    start_time = frame_time = time_ms();
    beat_time = -1;
    on_begin(pixels);
  }

  void end(const Pixels& pixels) {on_end(pixels); }

  void render(Pixels& pixels) {
    frame_time = time_ms();
    on_render(pixels);
  }

  void sync(int32_t start_t, int32_t beat_t) {
    start_time = start_t;
    beat_time = beat_t;
  }

  int32_t get_elapsed_time() const { return frame_time - start_time; }
  int32_t get_time_since_beat() const {
    return beat_time < 0 ? INT32_MAX : frame_time - beat_time;
  }

private:
  virtual void on_begin(const Pixels& pixels) {}
  virtual void on_end(const Pixels& pixels) {}
  virtual void on_render(Pixels& pixels) = 0;

  int32_t frame_time = -1;
  int32_t start_time = -1;
  int32_t beat_time = -1;
};


} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_H */