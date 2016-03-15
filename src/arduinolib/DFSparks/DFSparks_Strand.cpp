#include <Arduino.h>
#include "DFSparks_Color.h"
#include "DFSparks_Protocol.h"
#include "DFSparks_Strand.h"

namespace dfsparks {

inline uint32_t ntohl(uint32_t n) {
	return ((n & 0xFF) << 24)  | 
		   ((n & 0xFF00) << 8) |
		   ((n & 0xFF0000) >> 8) |
		   ((n & 0xFF000000) >> 24);
}

int Strand::begin(int pixelCount) {
	len = pixelCount;
	colors = new uint32_t[len];
	return 0;
}

void Strand::end() {
	delete[] colors;
}

int Strand::update(void *data, size_t size) {
  	using frame::Frame;
  	using frame::Type;
  	Frame frame;
  	if (size > sizeof(frame)) {
  		return -1;
  	}
  	memcpy(&frame, data, size);
  	if (ntohl(frame.header.frame_type) == static_cast<uint32_t>(Type::SOLID)) {
  		uint32_t color = ntohl(frame.data.solid.color);
  		for(int i=0; i<length(); ++i) {
  			colors[i]=color;
  		}
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

