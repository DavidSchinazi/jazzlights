#include <stdint.h>

extern "C" const char *proto_id() {
  return "DFSP/1.0";
}

extern "C" int render_solid_rgba(uint32_t color, uint32_t *data, int x, int y, int w, int h) {

}