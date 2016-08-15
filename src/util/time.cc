// Copyright 2016, Igor Chernyshev.

#include "util/time.h"

#include "util/logging.h"

uint64_t GetCurrentMillis() {
  struct timespec time;
  if (clock_gettime(CLOCK_MONOTONIC, &time) == -1) {
    REPORT_ERRNO("clock_gettime(monotonic)");
    CHECK(false);
  }
  return ((uint64_t) time.tv_sec) * 1000 + time.tv_nsec / 1000000;
}

void Sleep(double seconds) {
  struct timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  time.tv_sec += (int) seconds;
  time.tv_nsec += (long) ((seconds - (int) seconds) * 1000000000.0);
  while (time.tv_nsec >= 1000000000) {
    time.tv_sec += 1;
    time.tv_nsec -= 1000000000;
  }
  int err = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &time, nullptr);
  if (err != 0) {
    fprintf(stderr, "clock_nanosleep %d\n", err);
    CHECK(false);
  }
}

void SleepUs(int delay_us) {
  struct timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  time.tv_sec += delay_us / 1000000;
  time.tv_nsec += (delay_us % 1000000) * 1000;
  while (time.tv_nsec >= 1000000000) {
    time.tv_sec += 1;
    time.tv_nsec -= 1000000000;
  }
  int err = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &time, nullptr);
  if (err != 0) {
    fprintf(stderr, "clock_nanosleep %d\n", err);
    CHECK(false);
  }
}

void AddTimeMillis(struct timespec* time, uint64_t increment) {
  uint64_t nsec = (increment % 1000000) * 1000000 + time->tv_nsec;
  time->tv_sec += increment / 1000 + nsec / 1000000000;
  time->tv_nsec = nsec % 1000000000;
}
