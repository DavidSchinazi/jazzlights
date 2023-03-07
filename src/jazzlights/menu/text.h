#ifndef JL_TEXT_H
#define JL_TEXT_H

#include "jazzlights/config.h"

#if ATOM_MATRIX_SCREEN

#include <string>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

#define MATRIX_SIZE 25

bool displayText(const std::string& text, CRGB pixels[MATRIX_SIZE], CRGB textColor, CRGB backgroundColor,
                 Milliseconds offsetMillis);

}  // namespace jazzlights

#endif  // ATOM_MATRIX_SCREEN

#endif  // JL_TEXT_H
