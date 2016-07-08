#ifndef DFSPARKS_EFFECTS_H
#define DFSPARKS_EFFECTS_H
#include "dfsparks/effect.h"

namespace dfsparks {

class Effects {
public:
  void begin(Pixels &pixels);
  void end();

  Effect *get(const char *name);
  Effect *get(int idx);
  Effect &first();
  Effect *next(const Effect &ef);
  Effect *prev(const Effect &ef);
  int count() const;

private:
  Effect **all;
};

} // namespace dfsparks
#endif /* DFSPARKS_EFFECTS_H */
