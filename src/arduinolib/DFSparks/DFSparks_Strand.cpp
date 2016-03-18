#ifdef ARDUINO
# include <Arduino.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include "DFSparks_Color.h"
#include "DFSparks_Protocol.h"
#include "DFSparks_Strand.h"
#include <stdio.h>
#include <math.h>
namespace dfsparks {
using frame::Type;

// There's a bug that causes linker error on Arduino when using fmod with
// ESP8266 lib. This is a workaround we can use till it's fixed.
// https://github.com/esp8266/Arduino/issues/612
double _fmod(double v, double d) {
  return (v - floor(v / d)*d);
}

int Strand::begin(int pixelCount) {
	len = pixelCount;
  sequence = 0;
	colors = new uint32_t[len];
	return 0;
}

void Strand::end() {
	delete[] colors;
}

int Strand::update(void *data, size_t size) {
  if (!colors || !len) {
    return -1; // not initialized
  }

  switch(frame::decode_type(data, size)) {
    case Type::SOLID: {
      uint32_t color;
      frame::solid::decode(data, size, &color);  
      for(int i=0; i<length(); ++i) {
        colors[i]=color;
      }
    }
    break;

    case Type::RANDOM: {
      uint32_t new_sequence;
      frame::random::decode(data, size, &new_sequence);  
      if (new_sequence != sequence) {
        sequence = new_sequence;
        for(int i=0; i<length(); ++i) {
          colors[i]=hsl(rand()%1024/1024.0, 1, 1);
        }
      }
    }
    break;

    case Type::RAINBOW: {
        double from_hue, to_hue, saturation, lightness;
        if (!frame::rainbow::decode(data, size, &from_hue, &to_hue, &saturation, &lightness)) {
            return -1;
        }
        for(int i=0; i<length(); ++i) {
            double hue = _fmod(from_hue + (1 + to_hue - from_hue)*i/length(), 1);
            colors[i]=hsl(hue, saturation, lightness);
        }
    }
    break;

    default:
    ;

  }
    return 0;
}

int Strand::length() const { 
	return len; 
}

uint8_t Strand::red(int pixel) const {
	return chan_red(color(pixel));
}

uint8_t Strand::green(int pixel) const {
	return chan_green(color(pixel));
}

uint8_t Strand::blue(int pixel) const {
	return chan_blue(color(pixel));
}

uint32_t Strand::color(int pixel) const {
  return pixel >= 0 && pixel < len ? colors[pixel] : 0;
}

} // namespace dfsparks

