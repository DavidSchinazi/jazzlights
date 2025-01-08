#ifndef JL_UI_TEXT_H
#define JL_UI_TEXT_H

#include "jazzlights/config.h"

#if JL_IS_CONTROLLER(ATOM_MATRIX)

#include <string>

#include "jazzlights/fastled_wrapper.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

#define MATRIX_SIZE 25

bool displayText(const std::string& text, CRGB pixels[MATRIX_SIZE], CRGB textColor, CRGB backgroundColor,
                 Milliseconds offsetMillis);

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_MATRIX)

#endif  // JL_UI_TEXT_H
