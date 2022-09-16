#ifndef JAZZLIGHTS_RENDERERS_OPENPIXEL_H
#define JAZZLIGHTS_RENDERERS_OPENPIXEL_H
#include "jazzlights/renderer.hpp"

namespace jazzlights {

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


}  // namespace jazzlights
#endif  // JAZZLIGHTS_RENDERERS_OPENPIXEL_H

