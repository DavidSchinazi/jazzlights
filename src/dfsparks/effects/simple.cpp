#include "dfsparks/effects/simple.h"
#include "dfsparks/color.h"
#include "dfsparks/math.h"
namespace dfsparks {


void renderBlink(Pixels &pixels, const Frame &frame) {
  constexpr uint32_t color1 = 0xff0000;
  constexpr uint32_t color2 = 0x00ff00;
  constexpr int32_t period = 1000;
  pixels.fill(frame.timeElapsed / period % 2 ? color1 : color2);
}


void renderGlitter(Pixels &pixels, const Frame &frame) {
  constexpr int32_t hue_period = 8000;
  uint8_t cycleHue = 256 * frame.timeElapsed / hue_period;

  for (int i = 0; i < pixels.count(); ++i) {
      pixels.setColor(i, HslColor(cycleHue, 255, random8(5) * 63));
  }
}


void renderPlasma(Pixels &pixels, const Frame &frame) {
  constexpr int32_t speed = 30;
  int width = pixels.width();
  int height = pixels.height();

  uint8_t offset = speed * frame.timeElapsed / 255;
  int plasVector = offset * 16;

  // Calculate current center of plasma pattern (can be offscreen)
  int xOffset = cos8(plasVector / 256);
  int yOffset = sin8(plasVector / 256);

  for (int i = 0; i < pixels.count(); ++i) {
    Point p = pixels.coords(i);

    double xx = 16.0 * p.x / width;
    double yy = 16.0 * p.y / height;
    uint8_t hue = sin8(sqrt(square((xx - 7.5) * 10 + xOffset - 127) +
                            square((yy - 2) * 10 + yOffset - 127)) +
                       offset);

    pixels.setColor(i, HslColor(hue, 255, 255));
  }
}


void renderRainbow(Pixels &pixels, const Frame &frame) {
  constexpr int32_t period = 1800;
  int width = pixels.width();
  int height = pixels.height();

  uint8_t initial_hue = 255 - 255 * frame.timeElapsed / period;
  double maxd = sqrt(width * width + height * height) / 2;
  int cx = width / 2;
  int cy = height / 2;

  for (int i = 0; i < pixels.count(); ++i) {
    Point p = pixels.coords(i);

    int dx = p.x - cx;
    int dy = p.y - cy;
    double d = sqrt(dx * dx + dy * dy);
    uint8_t hue =
        (initial_hue + int32_t(255 * d / maxd)) % 255; 
    pixels.setColor(i, HslColor(hue, 240, 255));
  }
}

void renderRider(Pixels &pixels, const Frame &frame) {
  constexpr int32_t rider_period = 3000;
  constexpr int32_t hue_period = 10000;
  int width = pixels.width();

  uint8_t cycleHue = 256 * frame.timeElapsed / hue_period;
  uint8_t riderPos = 256 * frame.timeElapsed / rider_period;

  for (int i = 0; i < pixels.count(); ++i) {
    Point p = pixels.coords(i);

    // this is the same for all values of y, so we can optimize
    HslColor riderColor;
    {
      int brightness =
          absi(p.x * (256 / (width % 256)) - triwave8(riderPos) * 2 + 127) * 3;
      if (brightness > 255) {
        brightness = 255;
      }
      brightness = 255 - brightness;
      riderColor = HslColor(cycleHue, 255, brightness);
    }
    pixels.setColor(i, riderColor);
  }
}

void renderSlantbars(Pixels &pixels, const Frame &frame) {
  constexpr int32_t hue_period = 8000;
  constexpr int32_t mtn_period = 1000;
  uint8_t cycleHue = 256 * frame.timeElapsed / hue_period;
  uint8_t slantPos = 256 * frame.timeElapsed / mtn_period;
  int width = pixels.width();
  int height = pixels.height();

  for (int i = 0; i < pixels.count(); ++i) {
    Point p = pixels.coords(i);
    double xx = width < 8 ? p.x : 8.0 * p.x / width;
    double yy = height < 8 ? p.y : 8.0 * p.y / height;
    pixels.setColor(
        i, HslColor(cycleHue, 255, quadwave8(xx * 32 + yy * 32 + slantPos)));
  }
}

void renderTreesine(Pixels &pixels, const Frame &frame) {
    constexpr int32_t period = 8000;
    int width = pixels.width();
    int height = pixels.height();

    uint8_t sineOffset = 256 * frame.timeElapsed / period;

    for (int i = 0; i < pixels.count(); ++i) {
      Point p = pixels.coords(i);

      // Calculate "sine" waves with varying periods
      // sin8 is used for speed; cos8, quadwave8, or triwave8 would also work
      // here
      double xx = (width > 64 ? 8.0 : 4.0) * p.x / width;
      uint8_t sinDistanceR =
          qmul8(abs(p.y * (255 / height) - sin8(sineOffset * 9 + xx * 16)), 2);
      uint8_t sinDistanceG =
          qmul8(abs(p.y * (255 / height) - sin8(sineOffset * 10 + xx * 16)), 2);
      uint8_t sinDistanceB =
          qmul8(abs(p.y * (255 / height) - sin8(sineOffset * 11 + xx * 16)), 2);
      pixels.setColor(
          i, RgbaColor(255 - sinDistanceR, 255 - sinDistanceG, 255 - sinDistanceB));
    }
    sineOffset++; // byte will wrap from 255 to 0, matching sin8 0-255 cycle
  }


} // namespace dfsparks
