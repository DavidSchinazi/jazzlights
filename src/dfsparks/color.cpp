#include "dfsparks/color.h"
#include "dfsparks/math.h"
#define FAST_HSL 1

namespace dfsparks {

// This is an "accurate" version of color conversions using double prceision
// floating-points, should be pretty slow on ESP8266 chips
void rgb2hsl(uint32_t color, uint8_t* h, uint8_t* s, uint8_t* v) {
    double in_r = redChan(color)/255.0;
    double in_g = greenChan(color)/255.0;
    double in_b = blueChan(color)/255.0;
    double out_h, out_s, out_v;
    double      min, max, delta;

    min = in_r < in_g ? in_r : in_g;
    min = min  < in_b ? min  : in_b;

    max = in_r > in_g ? in_r : in_g;
    max = max  > in_b ? max  : in_b;

    out_v = max;                                
    delta = max - min;
    if (delta < 0.00001)
    {
        out_s = 0;
        out_h = 0; 
        goto ret;
    }
    if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
        out_s = (delta / max);                  // s
    } else {
        // if max is 0, then r = g = b = 0              
            // s = 0, v is undefined
        out_s = 0.0;
        out_h = NAN;                            // its now undefined
        goto ret;
    }
    if( in_r >= max )                           // > is bogus, just keeps compilor happy
        out_h = ( in_g - in_b ) / delta;        // between yellow & magenta
    else
    if( in_g >= max )
        out_h = 2.0 + ( in_b - in_r ) / delta;  // between cyan & yellow
    else
        out_h = 4.0 + ( in_r - in_g ) / delta;  // between magenta & cyan

    out_h *= 60.0;                              // degrees

    if(out_h < 0.0 )
        out_h += 360.0;

ret:
    *h = 255*out_h/360.0;
    *s = 255*out_s;
    *v = 255*out_v;
}

#ifndef FAST_HSL
uint32_t hsl(uint8_t h, uint8_t s, uint8_t v) {
    double in_h = 360*h/255.0, in_s = s/255.0, in_v = v/255.0;
    double      hh, p, q, t, ff;
    long        i;
    double out_r, out_g, out_b;

    if(in_s <= 0.0) {       
        return rgb(255*in_v, 255*in_v, 255*in_v);
    }
    hh = in_h;
    if(hh >= 360.0) hh = 0.0;
    hh /= 60.0;
    i = (long)hh;
    ff = hh - i;
    p = in_v * (1.0 - in_s);
    q = in_v * (1.0 - (in_s * ff));
    t = in_v * (1.0 - (in_s * (1.0 - ff)));

    switch(i) {
    case 0:
        out_r = in_v;
        out_g = t;
        out_b = p;
        break;
    case 1:
        out_r = q;
        out_g = in_v;
        out_b = p;
        break;
    case 2:
        out_r = p;
        out_g = in_v;
        out_b = t;
        break;

    case 3:
        out_r = p;
        out_g = q;
        out_b = in_v;
        break;
    case 4:
        out_r = t;
        out_g = p;
        out_b = in_v;
        break;
    case 5:
    default:
        out_r = in_v;
        out_g = p;
        out_b = q;
        break;
    }
    return rgb(255*out_r, 255*out_g, 255*out_b);     
}
#else
uint32_t hsl(uint8_t hsv_h, uint8_t hsv_s, uint8_t hsv_v) {
    uint8_t region, remainder, p, q, t, rgb_r, rgb_g, rgb_b;

    if (hsv_s == 0)
    {
      return rgb(hsv_v, hsv_v, hsv_v);
    }

    region = hsv_h / 43;
    remainder = (hsv_h - (region * 43)) * 6; 

    p = (hsv_v * (255 - hsv_s)) >> 8;
    q = (hsv_v * (255 - ((hsv_s * remainder) >> 8))) >> 8;
    t = (hsv_v * (255 - ((hsv_s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb_r = hsv_v; rgb_g = t; rgb_b = p;
            break;
        case 1:
            rgb_r = q; rgb_g = hsv_v; rgb_b = p;
            break;
        case 2:
            rgb_r = p; rgb_g = hsv_v; rgb_b = t;
            break;
        case 3:
            rgb_r = p; rgb_g = q; rgb_b = hsv_v;
            break;
        case 4:
            rgb_r = t; rgb_g = p; rgb_b = hsv_v;
            break;
        default:
            rgb_r = hsv_v; rgb_g = p; rgb_b = q;
            break;
    }

    return rgb(rgb_r, rgb_g, rgb_b);
}
#endif

} // namespace dfsparks
