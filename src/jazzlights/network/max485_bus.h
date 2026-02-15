#ifndef JL_NETWORK_MAX485_BUS_H
#define JL_NETWORK_MAX485_BUS_H

#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

#if JL_MAX485_BUS

void SetupMax485Bus(Milliseconds currentTime);

void RunMax485Bus(Milliseconds currentTime);

#else   // JL_MAX485_BUS
static inline void SetupMax485Bus(Milliseconds currentTime) { (void)currentTime; }
static inline void RunMax485Bus(Milliseconds currentTime) { (void)currentTime; }
#endif  // JL_MAX485_BUS

}  // namespace jazzlights

#endif  // JL_NETWORK_MAX485_BUS_H
