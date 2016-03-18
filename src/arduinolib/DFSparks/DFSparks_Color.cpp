#include "DFSparks_Color.h"

namespace dfsparks {

uint32_t scale(double r, double g, double b) {
    return rgb(
        static_cast<uint8_t>(r*255),
        static_cast<uint8_t>(g*255), 
        static_cast<uint8_t>(b*255)); 
}

uint32_t hsl(double h, double s, double v) {
    if (s == 0.0) return scale(v, v, v);
    int i = static_cast<int>(h*6.0) % 6;
    double f = (h*6.0) - i;
    double p = v*(1.0 - s);
    double q = v*(1.0 - s*f);
    double t = v*(1.0 - s*(1.0-f));
    if (i%6 == 0) return scale(v, t, p);
    if (i == 1) return scale(q, v, p);
    if (i == 2) return scale(p, v, t);
    if (i == 3) return scale(p, q, v);
    if (i == 4) return scale(t, p, v);
    /*if (i == 5)*/ return scale(v, p, q);
}

} // namespace dfsparks

