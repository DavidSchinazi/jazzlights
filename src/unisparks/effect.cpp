#include "unisparks/effect.h"
namespace unisparks {

Effect::Renderer* StatelessEffect::start(Pixels& pixels, void *buf, size_t bufsize) const {
  assert(sizeof(Renderer) <= bufsize);
  return new (buf) Renderer{*this, pixels};
}


// Effect::Renderer *BasicEffect::start(Pixels& pixels) const {
// 	return new Renderer(*this, pixels);
// }

// Effect::Renderer *BasicEffect::start(Pixels& pixels, void *buf, size_t bufsize) const {
// 	if (bufsize < sizeof(Renderer)) {
//           error("Can't start %s, render buffer is too small: %d < %d", name(), 
//             bufsize, sizeof(Renderer));
//           return nullptr;
//     }
// 	return new (buf) Renderer(*this, pixels);
// }

} // namespace unisparks
