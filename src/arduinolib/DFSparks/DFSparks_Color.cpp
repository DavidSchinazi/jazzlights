#include "DFSparks_Color.h"

namespace dfsparks {

float hue2rgb(float p, float q, float t) {
    if(t < 0) t += 1;
    if(t > 1) t -= 1;
    if(t < 1/6) return p + (q - p) * 6 * t;
    if(t < 1/2) return q;
    if(t < 2/3) return p + (q - p) * (2/3 - t) * 6;
    return p;
}

uint32_t hsl(float h, float s, float l) {
	h = h/=360.0; // convert from [0,360] to [0,1]
	double r,g,b;
	if (s == 0) {
		r = g = b = l; // achromatic
	} else {
        double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        double p = 2 * l - q;
        r = hue2rgb(p, q, h + 1/3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1/3);
	}
	return rgb(
		static_cast<uint8_t>(r*255),
		static_cast<uint8_t>(g*255), 
		static_cast<uint8_t>(b*255)); 
}

} // namespace dfsparks

