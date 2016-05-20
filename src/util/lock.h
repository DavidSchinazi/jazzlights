// Copyright 2016, Igor Chernyshev.

#ifndef UTIL_LOCK_H_
#define UTIL_LOCK_H_

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/logging.h"

class Autolock {
 public:
  Autolock(pthread_mutex_t& lock) : lock_(&lock) {
    int err = pthread_mutex_lock(lock_);
    if (err != 0) {
      fprintf(stderr, "Unable to acquire mutex: %d\n", err);
      CHECK(false);
    }
  }

  ~Autolock() {
    int err = pthread_mutex_unlock(lock_);
    if (err != 0) {
      fprintf(stderr, "Unable to release mutex: %d\n", err);
      CHECK(false);
    }
  }

 private:
  Autolock(const Autolock& src);
  Autolock& operator=(const Autolock& rhs);

  pthread_mutex_t* lock_;
};

#endif  // UTIL_LOCK_H_
