#ifndef UNISPARKS_RENDERER_H
#define UNISPARKS_RENDERER_H
#include "unisparks/util/geom.hpp"
#include "unisparks/util/time.hpp"
#include "unisparks/util/color.hpp"
#include "unisparks/util/stream.hpp"
#include "unisparks/util/log.hpp"

namespace unisparks {

class Renderer {
 public:
  Renderer() = default;
  virtual ~Renderer() = default;

  virtual void render(InputStream<Color>& colors) = 0;
};


} // namespace unisparks
#endif /* UNISPARKS_RENDERER_H */
