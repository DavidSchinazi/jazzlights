#ifndef DFSPARKS_COLOR_H
#define DFSPARKS_COLOR_H
#include <stdint.h>
#include <string.h>

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
  return (static_cast<uint32_t>(0xFF - a) << 24) |
         static_cast<uint32_t>(r) << 16 | static_cast<uint32_t>(g) << 8 | (b);
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
  return rgba(r, g, b, 255);
}

/**
 * Create color using HSL model
 *
 * @param   h hue, [0,255]
 * @param   s saturation, [0,255]
 * @param   l lightness, [0,255]
 * @return  encoded color
 */
uint32_t hsl(uint8_t h, uint8_t s, uint8_t l);

inline uint8_t chan_blue(uint32_t color) { return color & 0xFF; }

inline uint8_t chan_green(uint32_t color) { return (color >> 8) & 0xFF; }

inline uint8_t chan_red(uint32_t color) { return (color >> 16) & 0xFF; }

inline uint8_t chan_alpha(uint32_t color) {
  return 0xFF - ((color >> 24) & 0xFF);
}

} // namespace dfsparks
#endif /* DFSPARKS_COLOR_H */