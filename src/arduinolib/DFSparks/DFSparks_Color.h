// DiscoFish wearables protocol
#ifndef DFSPARKS_COLOR_H
#define DFSPARKS_COLOR_H

namespace dfsparks {
	inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		return (r<<24)|(g<<16)|(b<<8)|(a);
	}

	inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
		return rgba(r,g,b,255);
	}

	inline uint8_t chan_alpha(uint32_t color) {
		return color & 0xFF;
	}

	inline uint8_t chan_blue(uint32_t color) {
		return (color>>8) & 0xFF;
	}

	inline uint8_t chan_green(uint32_t color) {
		return (color>>16) & 0xFF;
	}

	inline uint8_t chan_red(uint32_t color) {
		return (color>>24) & 0xFF;
	}
}

#endif /* DFSPARKS_COLOR_H */