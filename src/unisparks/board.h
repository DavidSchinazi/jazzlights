#ifndef UNISPARKS_BOARD_H
#define UNISPARKS_BOARD_H

#include "unisparks/config.h"

#ifndef ORANGE_VEST
#  define ORANGE_VEST 0
#endif // ORANGE_VEST
#ifndef CAMP_SIGN
#  define CAMP_SIGN 0
#endif // CAMP_SIGN
#ifndef YELLOW_VEST_15
#  define YELLOW_VEST_15 0
#endif // YELLOW_VEST_15
#ifndef YELLOW_VEST_14
#  define YELLOW_VEST_14 0
#endif // YELLOW_VEST_14
#ifndef GUPPY
#  define GUPPY 0
#endif // GUPPY

#if defined(ESP32)
#  define LED_PIN  26
#elif defined(ESP8266)
#  define LED_PIN  5
#endif

#if ORANGE_VEST
#  define MATRIX_WIDTH 20
#  define MATRIX_HEIGHT 19
#  define LEDNUM 360
#endif // ORANGE_VEST

#if CAMP_SIGN
#  define FIRST_BRIGHTNESS 255
#  define MATRIX_WIDTH 30
#  define MATRIX_HEIGHT 30
#  define LEDNUM 900
#endif // CAMP_SIGN

#if YELLOW_VEST_15 || YELLOW_VEST_14
#  define MATRIX_WIDTH 15
#  define MATRIX_HEIGHT 16
#  define LEDNUM 215
#endif // YELLOW_VEST_15

#if GUPPY
#  define MATRIX_WIDTH 15
#  define MATRIX_HEIGHT 20
#  define LEDNUM 300
#endif // GUPPY

extern const int pixelMap[];

#endif // UNISPARKS_BOARD_H
