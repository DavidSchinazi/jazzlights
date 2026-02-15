#ifndef JL_NETWORK_UART_H
#define JL_NETWORK_UART_H

#include "jazzlights/config.h"
#include "jazzlights/util/time.h"

namespace jazzlights {

#if JL_UART

void SetupUart(Milliseconds currentTime);

void RunUart(Milliseconds currentTime);

#else   // JL_UART
static inline void SetupUart(Milliseconds currentTime) { (void)currentTime; }
static inline void RunUart(Milliseconds currentTime) { (void)currentTime; }
#endif  // JL_UART

}  // namespace jazzlights

#endif  // JL_NETWORK_UART_H
