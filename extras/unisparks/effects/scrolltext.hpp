#ifndef UNISPARKS_EFFECTS_PLASMA_H
#define UNISPARKS_EFFECTS_PLASMA_H
#include "unisparks/effects/functional.h"

namespace unisparks {

// constexpr auto scrolltext = [](const char* str) {
//   return effect([](const Frame & frame) {
//     constexpr int32_t speed = 30;
//     int width = frame.size.width;
//     int height = frame.size.height;

//     uint8_t offset = speed * frame.time / 255;
//     int plasVector = offset * 16;

//     // Calculate current center of plasma pattern (can be offscreen)
//     int xOffset = cos8(plasVector / 256);
//     int yOffset = sin8(plasVector / 256);

//     return [ = ](Point p) -> Color {
//       double xx = 16.0 * p.x / width;
//       double yy = 16.0 * p.y / height;
//       uint8_t hue = sin8(sqrt(square((xx - 7.5) * 10 + xOffset - 127) +
//       square((yy - 2) * 10 + yOffset - 127)) +
//       offset);

//       return HslColor(hue, 255, 255);
//     };
//   });
// };

} // namespace unisparks
#endif /* UNISPARKS_EFFECTS_PLASMA_H */

