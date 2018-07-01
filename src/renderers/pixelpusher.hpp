#ifndef UNISPARKS_RENDERERS_PIXELPUSHER_H
#define UNISPARKS_RENDERERS_PIXELPUSHER_H
#include <netinet/in.h>

#include "unisparks/renderer.hpp"

namespace unisparks {

struct PixelPusher : Renderer {
  PixelPusher(const char* host, int port, int strip, int throttle = 1000 / 30);
  void render(InputStream<Color>& pixelColors) override;

  const char* host;
  int port;
  int strip;
  sockaddr_in addr;
  int throttle;
  int fd;
  Milliseconds lastTxTime = -1;
};


} // namespace unisparks
#endif /* UNISPARKS_RENDERERS_PIXELPUSHER_H */
