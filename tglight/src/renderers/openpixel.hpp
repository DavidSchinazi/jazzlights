#ifndef UNISPARKS_RENDERERS_OPENPIXEL_H
#define UNISPARKS_RENDERERS_OPENPIXEL_H
#include "unisparks/renderer.hpp"

namespace unisparks {

class OpenPixelWriter : public Renderer {
 public:
  OpenPixelWriter(const char* host, int port, uint8_t channel = 0);
  OpenPixelWriter(OpenPixelWriter&&); // = default;
  OpenPixelWriter(const OpenPixelWriter&) = delete;
  ~OpenPixelWriter();
  OpenPixelWriter& operator=(const OpenPixelWriter&) = delete;
  OpenPixelWriter& operator=(OpenPixelWriter&&) = default;

 private:
  void render(InputStream<Color>& pixelColors) override;

  uint8_t channel_;
  int8_t opcSink_;
  int throttle = 50;
  Milliseconds lastTxTime = -1;

};


} // namespace unisparks
#endif /* UNISPARKS_RENDERERS_OPENPIXEL_H */

