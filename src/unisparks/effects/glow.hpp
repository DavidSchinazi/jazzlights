#ifndef UNISPARKS_EFFECTS_GLOW_H
#define UNISPARKS_EFFECTS_GLOW_H
#include "unisparks/effects/functional.hpp"
#include "unisparks/util/color.hpp"
#include "unisparks/util/math.hpp"

namespace unisparks {

auto glow = []() {
  return effect("glow", [ = ](const Frame & frame) {
		constexpr uint32_t period = 3000;
		constexpr uint32_t half_low_time = 10;
		constexpr uint32_t half_high_time = 400;
		constexpr uint32_t half_period = period / 2;
		uint32_t time_in_period = frame.time % period;
		if (time_in_period > half_period) {
			time_in_period = period - time_in_period;
		}
		constexpr uint8_t min_intensity = 25;
		constexpr uint8_t max_intensity = 255;
		constexpr uint8_t intensity_delta = max_intensity - min_intensity;
		uint8_t intensity;
		if (time_in_period <= half_low_time) {
			intensity = min_intensity;
		} else if (time_in_period > half_low_time && time_in_period < (half_period - half_high_time)) {
			const uint32_t ramp_time = time_in_period - half_low_time;
			constexpr uint32_t ramp_length = half_period - (half_low_time + half_high_time);
			intensity = (ramp_time * intensity_delta / ramp_length) + min_intensity;
		} else {
			intensity = max_intensity;
		}
		const Color color(RgbColor(intensity, 0, 0));
    return [ = ](const Pixel& /*pt*/) -> Color {
      return color;
    };
  });
};

} // namespace unisparks
#endif // UNISPARKS_EFFECTS_GLOW_H
