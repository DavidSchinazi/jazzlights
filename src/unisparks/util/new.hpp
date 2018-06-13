#ifndef UNISPARKS_NEW_H
#define UNISPARKS_NEW_H

// <new> header is not available on all boards, so we'll
// declare placement new/delete operators here
#ifdef ARDUINO_AVR_UNO
inline void* operator new(size_t, void* p) noexcept {return p;}
inline void* operator new[](size_t, void* p) noexcept {return p;}
inline void operator delete(void*, void*) noexcept {}
inline void operator delete[](void*, void*) noexcept {}
#else
#include <new>
#endif

#endif /* UNISPARKS_NEW_H */

