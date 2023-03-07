#include "jazzlights/fastled_wrapper.h"

#if !RENDERABLE

extern const TProgmemRGBPalette16 CloudColors_p FL_PROGMEM = {                             //
    CRGB::Blue,      CRGB::DarkBlue, CRGB::DarkBlue,  CRGB::DarkBlue,                      //
    CRGB::DarkBlue,  CRGB::DarkBlue, CRGB::DarkBlue,  CRGB::DarkBlue,                      //
    CRGB::Blue,      CRGB::DarkBlue, CRGB::SkyBlue,   CRGB::SkyBlue,                       //
    CRGB::LightBlue, CRGB::White,    CRGB::LightBlue, CRGB::SkyBlue};                      //
extern const TProgmemRGBPalette16 OceanColors_p FL_PROGMEM = {                             //
    CRGB::MidnightBlue, CRGB::DarkBlue,   CRGB::MidnightBlue, CRGB::Navy,                  //
    CRGB::DarkBlue,     CRGB::MediumBlue, CRGB::SeaGreen,     CRGB::Teal,                  //
    CRGB::CadetBlue,    CRGB::Blue,       CRGB::DarkCyan,     CRGB::CornflowerBlue,        //
    CRGB::Aquamarine,   CRGB::SeaGreen,   CRGB::Aqua,         CRGB::LightSkyBlue};         //
extern const TProgmemRGBPalette16 ForestColors_p FL_PROGMEM = {                            //
    CRGB::DarkGreen,  CRGB::DarkGreen,        CRGB::DarkOliveGreen,   CRGB::DarkGreen,     //
    CRGB::Green,      CRGB::ForestGreen,      CRGB::OliveDrab,        CRGB::Green,         //
    CRGB::SeaGreen,   CRGB::MediumAquamarine, CRGB::LimeGreen,        CRGB::YellowGreen,   //
    CRGB::LightGreen, CRGB::LawnGreen,        CRGB::MediumAquamarine, CRGB::ForestGreen};  //
extern const TProgmemRGBPalette16 RainbowColors_p FL_PROGMEM = {                           //
    0xFF0000, 0xD52A00, 0xAB5500, 0xAB7F00,                                                //
    0xABAB00, 0x56D500, 0x00FF00, 0x00D52A,                                                //
    0x00AB55, 0x0056AA, 0x0000FF, 0x2A00D5,                                                //
    0x5500AB, 0x7F0081, 0xAB0055, 0xD5002B};                                               //
extern const TProgmemRGBPalette16 PartyColors_p FL_PROGMEM = {                             //
    0x5500AB, 0x84007C, 0xB5004B, 0xE5001B,                                                //
    0xE81700, 0xB84700, 0xAB7700, 0xABAB00,                                                //
    0xAB5500, 0xDD2200, 0xF2000E, 0xC2003E,                                                //
    0x8F0071, 0x5F00A1, 0x2F00D0, 0x0007F9};                                               //
extern const TProgmemRGBPalette16 HeatColors_p FL_PROGMEM = {                              //
    0x000000, 0x330000, 0x660000, 0x990000,                                                //
    0xCC0000, 0xFF0000, 0xFF3300, 0xFF6600,                                                //
    0xFF9900, 0xFFCC00, 0xFFFF00, 0xFFFF33,                                                //
    0xFFFF66, 0xFFFF99, 0xFFFFCC, 0xFFFFFF};                                               //

#endif  // !RENDERABLE

extern const TProgmemRGBPalette16 JLLavaColors_p FL_PROGMEM = {  //
    CRGB::Black,   CRGB::Maroon,  CRGB::Black,  CRGB::Maroon,    //
    CRGB::DarkRed, CRGB::DarkRed, CRGB::Maroon, CRGB::DarkRed,   //
    CRGB::DarkRed, CRGB::DarkRed, CRGB::Red,    CRGB::Orange,    //
    CRGB::White,   CRGB::Orange,  CRGB::Red,    CRGB::DarkRed};  //
