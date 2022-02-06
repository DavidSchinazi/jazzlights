#include "renderers/pixelpusher.hpp"

#include <vector>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "unisparks/util/log.hpp"
#include "unisparks/util/time.hpp"

namespace unisparks {

static constexpr int HACKY_HARD_CODED_NUM_OF_LEDS=400;

PixelPusher::PixelPusher(const char* h, int p, int s, int t) : host(h),
  port(p), strip(s), throttle(t), fd(0) {
  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    fatal("can't create client socket: %s", strerror(errno));
  }

  struct hostent* hp;
  hp = gethostbyname(host);
  if (!hp) {
    fatal("can't connect to %s: %s", host, strerror(errno));
  }

  bzero((char*) &addr, sizeof(addr));
  addr.sin_family = AF_INET;
  bcopy((char*)hp->h_addr,
        (char*)&addr.sin_addr.s_addr, hp->h_length);
  addr.sin_port = htons(port);
}

void PixelPusher::render(InputStream<Color>& pixelColors) {
  assert(fd);

  Milliseconds currTime = timeMillis();
  if (lastTxTime > 0 && currTime - lastTxTime < throttle) {
    return;
  }
  lastTxTime = currTime;

  static uint32_t sequence = 1000000;

  std::vector<uint8_t> buf;
  buf.push_back((sequence >> 24) & 0xFF);
  buf.push_back((sequence >> 16) & 0xFF);
  buf.push_back((sequence >> 8) & 0xFF);
  buf.push_back(sequence & 0xFF);
  buf.push_back(strip & 0xFF);
  sequence++;

  int pxcnt = 0;
  for (auto color : pixelColors) {
    auto rgb = color.asRgb();
    buf.push_back(rgb.red);
    buf.push_back(rgb.green);
    buf.push_back(rgb.blue);
    pxcnt++;
    if (pxcnt > HACKY_HARD_CODED_NUM_OF_LEDS) {
      break;
    }
  }
  while (pxcnt < HACKY_HARD_CODED_NUM_OF_LEDS) {
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    pxcnt++;
  }

  if (sendto(fd, buf.data(), buf.size(), 0, (struct sockaddr*)&addr,
             sizeof(addr)) != static_cast<ssize_t>(buf.size())) {
    error("Can't send %d bytes to PixelPusher at %s:%d on socket %d: %s",
          buf.size(),
          host, port, fd, strerror(errno));
  }
  debug("Sent strip %d (%d pixels, %d bytes) to PixelPusher at %s:%d on socket %d",
        strip, pxcnt, buf.size(), host, port, fd);
}

} // namespace unisparks
