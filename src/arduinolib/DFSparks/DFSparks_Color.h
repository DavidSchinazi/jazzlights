#ifndef DFSPARKS_COLOR_H
#define DFSPARKS_COLOR_H
#ifndef ARDUINO
# include <stdint.h>
# include <string.h>
#endif

namespace dfsparks {

	/**
 	 * Create color using RGB channel values
 	 *
 	 * @param   r red channel value, [0, 255]
 	 * @param   g green channel value, [0, 255]
 	 * @param   b blue channel value, [0, 255]
	 * @return  encoded color	
	 */
	inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		return (r<<24)|(g<<16)|(b<<8)|(a);
	}

	/**
 	 * Create color using RGBA channel values
 	 *
 	 * @param   r red channel value, [0, 255]
 	 * @param   g green channel value, [0, 255]
 	 * @param   b blue channel value, [0, 255]
 	 * @param   a alpha (transparency) channel value, [0, 255]. Zero means fully 
 	 *            transparent, 255 - fully opaque.
	 * @return  encoded color	
	 */
	inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
		return rgba(r,g,b,255);
	}

	/**
 	 * Create color using HSL model
 	 *
 	 * @param   h hue, [0,1]
 	 * @param   s saturation, [0,1]
 	 * @param   l lightness, [0,1]
	 * @return  encoded color	
	 */
	uint32_t hsl(float h, float s, float l);

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