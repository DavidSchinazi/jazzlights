#ifndef JAZZLIGHTS_RENDERER_H
#define JAZZLIGHTS_RENDERER_H
#include "jazzlights/util/geom.hpp"
#include "jazzlights/util/time.hpp"
#include "jazzlights/util/color.hpp"
#include "jazzlights/util/stream.hpp"
#include "jazzlights/util/log.hpp"

namespace jazzlights {

class Renderer {
 public:
  Renderer() = default;
  virtual ~Renderer() = default;

  virtual void render(InputStream<Color>& colors) = 0;
};


}  // namespace jazzlights
#endif  // JAZZLIGHTS_RENDERER_H
