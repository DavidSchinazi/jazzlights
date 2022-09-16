#ifndef JAZZLIGHTS_RENDERERS_PIXELPUSHER_H
#define JAZZLIGHTS_RENDERERS_PIXELPUSHER_H
#include <netinet/in.h>

#include "jazzlights/renderer.h"

namespace jazzlights {

class PixelPusher : public Renderer {
public:
  PixelPusher(const char* host, int port, int strip, int throttle, int32_t controller, int32_t group);
  void render(InputStream<Color>& pixelColors) override;

private:
  const char* host;
  int port;
  int strip;
  sockaddr_in addr;
  int throttle;
  int32_t controller_;
  int32_t group_;
  int fd;
  Milliseconds lastTxTime = -1;
  // Milliseconds lastReconnectTime = -1;
};


}  // namespace jazzlights
#endif  // JAZZLIGHTS_RENDERERS_PIXELPUSHER_H
