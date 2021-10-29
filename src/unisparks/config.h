#ifndef UNISPARKS_CONFIG_H
#define UNISPARKS_CONFIG_H

#ifndef WEARABLE
#  define WEARABLE 0
#endif // WEARABLE

// Pick which vest to build here.
#if !defined(CAMPSIGN) && !defined(ROPELIGHT) && !defined(ORANGE_VEST) && !defined(HAMMER) && !defined(GUPPY)
#  define ORANGE_VEST 1
// #define CAMP_SIGN 1
// #define GUPPY 1
#endif

#ifndef CAMPSIGN
#  define CAMPSIGN 0
#endif // CAMPSIGN

#ifndef ROPELIGHT
#  define ROPELIGHT 0
#endif // ROPELIGHT

// Choose whether to enable button lock.
#ifndef BUTTON_LOCK
#  define BUTTON_LOCK 0
#endif // BUTTON_LOCK

#ifndef GLOW_ONLY
#  define GLOW_ONLY 0
#endif // GLOW_ONLY

#endif // UNISPARKS_CONFIG_H
