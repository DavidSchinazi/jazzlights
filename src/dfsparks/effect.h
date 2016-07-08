#ifndef DFSPARKS_EFFECT_H
#define DFSPARKS_EFFECT_H
#include "dfsparks/pixels.h"
#include "dfsparks/timer.h"
#include <stdint.h>

namespace dfsparks {

/**
 * Effect base class.
 */
class Effect {
public:
  Effect() noexcept = default;
  virtual ~Effect() noexcept = default;

  void begin(Pixels &px);
  void end();
  void restart();
  void knock();
  void sync(int32_t start_t, int32_t beat_t);
  void render();

  Effect &set_name(const char *n);

  int32_t get_start_time() const { return start_time; }

  int32_t get_beat_time() const { return beat_time; }

  const char *get_name() const { return name; }

  int32_t get_preferred_duration() const { return 10000; }

protected:
  int32_t get_elapsed_time() const { return frame_time - start_time; }
  int32_t get_time_since_beat() const { return frame_time - beat_time; }

private:
  virtual void on_begin(const Pixels &) {}
  virtual void on_end(const Pixels &) {}
  virtual void on_render(Pixels &) = 0;

  Pixels *pixels = nullptr;
  int32_t frame_time = -1;
  int32_t start_time = -1;
  int32_t beat_time = 0;
  const char *name = "unnamed";
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_H */
