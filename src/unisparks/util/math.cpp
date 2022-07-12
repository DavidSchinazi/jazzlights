/**
 * Copyright 2018 Unisparks Project Developers
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * This file uses code from FastLED library (https://github.com/FastLED/FastLED)
 * licensed under the following terms:
 *
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2013 FastLED
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of
 *  this software and associated documentation files (the "Software"), to deal in
 *  the Software without restriction, including without limitation the rights to
 *  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 *  the Software, and to permit persons to whom the Software is furnished to do so,
 *  subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 *  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 *  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 *  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 *  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "unisparks/util/math.hpp"

namespace unisparks {
namespace internal {

///         square root for 16-bit integers
///         About three times faster and five times smaller
///         than Arduino's general sqrt on AVR.
uint8_t sqrt16(uint16_t x) {
  if (x <= 1) {
    return x;
  }

  uint8_t low = 1; // lower bound
  uint8_t hi, mid;

  if (x > 7904) {
    hi = 255;
  } else {
    hi = (x >> 5) + 8; // initial estimate for upper bound
  }

  do {
    mid = (low + hi) >> 1;
    if ((uint16_t)(mid * mid) > x) {
      hi = mid - 1;
    } else {
      if (mid == 255) {
        return 255;
      }
      low = mid + 1;
    }
  } while (hi >= low);

  return low - 1;
}

} // namespace internal
} // namespace unisparks
