#ifndef UNISPARKS_EFFECT_SYSTEM_H
#define UNISPARKS_EFFECT_SYSTEM_H
#include "unisparks/effect.h"
#include "unisparks/network.h"
namespace unisparks {

class NetworkStatus : public StatelessEffect {
public:
  static const char *name;
  NetworkStatus(Network& net) : StatelessEffect(name), network(net) {}

private:
   void render(Pixels& pixels, const Frame& frame) const final;
   Network &network;
};


} // namespace unisparks
#endif /* UNISPARKS_EFFECT_SYSTEM_H */