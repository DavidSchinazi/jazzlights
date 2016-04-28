#ifndef DFSPARKS_EFFECT_H
#define DFSPARKS_EFFECT_H
#include <DFSparks_Timer.h>

namespace dfsparks {

// TODO: Move logic that does not need template parameters to
// base class
template <typename Pixels> class Effect {
public:
  Effect() {}
  virtual ~Effect() {}

  void begin() { start_time = time_ms(); on_begin(); };
  void end() { on_end(); };
  void render(Pixels &pixels) { on_render(pixels); }

  int32_t get_elapsed_time() const { return time_ms() - start_time; }
  void sync_start_time(int32_t t) { start_time = t; }

private:
  virtual void on_begin() {}
  virtual void on_end() {}
  virtual void on_render(Pixels &) = 0;

  int32_t start_time;
};

} // namespace dfsparks

#endif /* DFSPARKS_EFFECT_H */