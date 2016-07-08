#include "dfsparks/palette.h"
#if defined(__AVR_ARCH__)
#include <avr/pgmspace.h>
#elif defined(ARDUINO)
#include <pgmspace.h>
#else
#define PROGMEM
#endif

namespace dfsparks {
// Courtecy of FastLED library, https://github.com/FastLED/FastLED
//
// The MIT License (MIT)
//
// Copyright (c) 2013 FastLED
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
// of
// the Software, and to permit persons to whom the Software is furnished to do
// so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
// OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

/// HSV Rainbow
const Palette16 RAINBOW_COLORS PROGMEM = {
    {0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00, 0xABAB00, 0x56D500, 0x00FF00,
     0x00D52A, 0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5, 0x5500AB, 0x7F0081,
     0xAB0055, 0xD5002B}};

/// HSV Rainbow colors with alternatating stripes of black
const Palette16 RAINBOW_STRIPE_COLORS PROGMEM = {
    {0xFF0000, 0x000000, 0xAB5500, 0x000000, 0xABAB00, 0x000000, 0x00FF00,
     0x000000, 0x00AB55, 0x000000, 0x0000FF, 0x000000, 0x5500AB, 0x000000,
     0xAB0055, 0x000000}};

/// HSV color ramp: blue purple ping red orange yellow (and back)
/// Basically, everything but the greens, which tend to make
/// people's skin look unhealthy.  This palette is good for
/// lighting at a club or party, where it'll be shining on people.
const Palette16 PARTY_COLORS PROGMEM = {
    {0x5500AB, 0x84007C, 0xB5004B, 0xE5001B, 0xE81700, 0xB84700, 0xAB7700,
     0xABAB00, 0xAB5500, 0xDD2200, 0xF2000E, 0xC2003E, 0x8F0071, 0x5F00A1,
     0x2F00D0, 0x0007F9}};

/// Approximate "black body radiation" palette, akin to
/// the FastLED 'HeatColor' function.
/// Recommend that you use values 0-240 rather than
/// the usual 0-255, as the last 15 colors will be
/// 'wrapping around' from the hot end to the cold end,
/// which looks wrong.
const Palette16 HEAT_COLORS PROGMEM = {
    {0x000000, 0x330000, 0x660000, 0x990000, 0xCC0000, 0xFF0000, 0xFF3300,
     0xFF6600, 0xFF9900, 0xFFCC00, 0xFFFF00, 0xFFFF33, 0xFFFF66, 0xFFFF99,
     0xFFFFCC, 0xFFFFFF}};

} // namespace dfsparks
