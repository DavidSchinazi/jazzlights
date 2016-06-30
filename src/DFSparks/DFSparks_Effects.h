#ifndef DFSPARKS_EFFECTS_H
#define DFSPARKS_EFFECTS_H
#include <DFSparks_Effect.h>
#include <DFSparks_Effect_Blink.h>
#include <DFSparks_Effect_Flame.h>
#include <DFSparks_Effect_Flame.h>
#include <DFSparks_Effect_Plasma.h>
#include <DFSparks_Effect_Rainbow.h>
#include <DFSparks_Effect_Rider.h>
#include <DFSparks_Effect_Threesine.h>

namespace dfsparks {

class Effects {
public:
  void begin(Pixels &pixels);
  void end(Pixels &pixels);

  Effect *get(int32_t id);
  int count() const;

  Blink slow_blink = {1000, 0xff0000, 0x00ff00};
  Blink fast_blink = {75, 0xff0000, 0x00ff00};
  Rainbow radiate_rainbow = {1000};
  Threesine threesine = {10};
  Plasma plasma = {30};
  Rider rider = {100};
  Flame flame = {};
};

} // namespace dfsparks
#endif /* DFSPARKS_EFFECTS_H */