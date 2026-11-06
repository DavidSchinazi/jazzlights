#ifndef JL_SHIM_FREERTOS_SEMPHR_H
#define JL_SHIM_FREERTOS_SEMPHR_H

#include <condition_variable>
#include <mutex>
#include <new>

#include "freertos/FreeRTOS.h"

// Only supports binary semaphores, and waiting forever.
struct JlShimSemaphore {
  std::mutex mutex;
  std::condition_variable available;
  bool given = false;
};
using SemaphoreHandle_t = JlShimSemaphore*;

inline SemaphoreHandle_t xSemaphoreCreateBinary() { return new (std::nothrow) JlShimSemaphore(); }

inline BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t /*ticks*/) {
  std::unique_lock<std::mutex> lock(semaphore->mutex);
  semaphore->available.wait(lock, [semaphore]() { return semaphore->given; });
  semaphore->given = false;
  return pdTRUE;
}

inline BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
  std::lock_guard<std::mutex> lock(semaphore->mutex);
  if (semaphore->given) { return pdFALSE; }
  semaphore->given = true;
  semaphore->available.notify_one();
  return pdTRUE;
}

#endif  // JL_SHIM_FREERTOS_SEMPHR_H
