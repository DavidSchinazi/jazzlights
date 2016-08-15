// Copyright 2016, Igor Chernyshev.

#ifndef UTIL_TIME_H_
#define UTIL_TIME_H_

#include <stdint.h>
#include <time.h>

uint64_t GetCurrentMillis();

void Sleep(double seconds);
void SleepUs(int delay_us);

void AddTimeMillis(struct timespec* time, uint64_t increment);

#endif  // UTIL_TIME_H_
