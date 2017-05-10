#ifndef DFSPARKS_EFFECT_FLAME_H
#define DFSPARKS_EFFECT_FLAME_H
#include <stdlib.h>
#include "dfsparks/effect.h"
namespace dfsparks {

void renderFlame(Pixels& pixels, const Frame& frame, uint8_t* heat);

class Flame : public StatefulEffect<uint8_t> {
public:
    using StatefulEffect::StatefulEffect;

private:
    size_t contextSize(const Pixels& pixels) const final {return sizeof(uint8_t)*pixels.width()*pixels.height();}
    void begin(const Pixels& pixels, uint8_t &ctx) const final {memset(&ctx, 0, contextSize(pixels));}
    void end(const Pixels&, uint8_t &) const final {}
    void render(Pixels& pixels, const Frame& frame, uint8_t& ctx) const final {renderFlame(pixels, frame, &ctx);}
};

} // namespace dfsparks
#endif /* DFSPARKS_EFFECT_FLAME_H */