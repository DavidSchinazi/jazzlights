#ifndef JAZZLIGHTS_TEXT_H
#define JAZZLIGHTS_TEXT_H

#include <Jazzlights.h>

#if ATOM_MATRIX_SCREEN

namespace jazzlights {

#define MATRIX_SIZE 25

bool displayText(const std::string& text,
                 CRGB pixels[MATRIX_SIZE],
                 CRGB textColor,
                 CRGB backgroundColor,
                 Milliseconds offsetMillis);

}  // namespace jazzlights

#endif // ATOM_MATRIX_SCREEN

#endif  // JAZZLIGHTS_TEXT_H
