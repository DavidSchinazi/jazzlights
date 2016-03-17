#ifdef ARDUINO
# include <Arduino.h>
#else
# include <cstdint>
#endif
#include "DFSparks_Color.h"
#include "DFSparks_Protocol.h"
#include "DFSparks_Strand.h"

namespace dfsparks {
using frame::Type;

int Strand::begin(int pixelCount) {
	len = pixelCount;
	colors = new uint32_t[len];
	return 0;
}

void Strand::end() {
	delete[] colors;
}

int Strand::update(void *data, size_t size) {
  switch(frame::decode_type(data, size)) {
    case Type::SOLID: {
      uint32_t color;
      frame::solid::decode(data, size, &color);  
      for(int i=0; i<length(); ++i) {
        colors[i]=color;
      }
    }
    return 1;

    default:
    return 0;
  }
	return 0;	
}

int Strand::length() const { 
	return len; 
}

uint8_t Strand::red(int pixel) const {
	return chan_red(colors[pixel]);
}

uint8_t Strand::green(int pixel) const {
	return chan_green(colors[pixel]);
}

uint8_t Strand::blue(int pixel) const {
	return 0; chan_blue(colors[pixel]);
}

} // namespace dfsparks

