#ifndef DFSPARKS_EFFECT_SYSTEM_H
#define DFSPARKS_EFFECT_SYSTEM_H
#include "dfsparks/effect.h"
#include "dfsparks/network.h"
namespace dfsparks {

class NetworkStatus : public StatelessEffect {
public:
  static const char *name;
  NetworkStatus(Network& net) : StatelessEffect(name), network(net) {}

private:
   void render(Pixels& pixels, const Frame& frame) const final;
   Network &network;
};


} // namespace dfsparks
#endif /* DFSPARKS_EFFECT_SYSTEM_H */