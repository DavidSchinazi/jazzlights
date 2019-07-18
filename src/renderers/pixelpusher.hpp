#ifndef UNISPARKS_RENDERERS_PIXELPUSHER_H
#define UNISPARKS_RENDERERS_PIXELPUSHER_H
#include <netinet/in.h>

#include "unisparks/renderer.hpp"

namespace unisparks {

class PixelPusher : public Renderer {
public:
  PixelPusher(const char* host, int port, int strip, int throttle = 1000 / 30);
  void render(InputStream<Color>& pixelColors) override;

private:
  const char* host;
  int port;
  int strip;
  sockaddr_in addr;
  int throttle;
  int fd;
  Milliseconds lastTxTime = -1;
  Milliseconds lastReconnectTime = -1;
};


} // namespace unisparks
#endif /* UNISPARKS_RENDERERS_PIXELPUSHER_H */
