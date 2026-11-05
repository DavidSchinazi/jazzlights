#ifndef JL_SHIM_FREERTOS_TASK_H
#define JL_SHIM_FREERTOS_TASK_H

#include <pthread.h>

#include <chrono>
#include <thread>

#include "freertos/FreeRTOS.h"

// Only supports a task deleting itself, which ends its thread.
inline void vTaskDelete(TaskHandle_t /*task*/) { pthread_exit(nullptr); }

inline void vTaskDelay(TickType_t ticks) { std::this_thread::sleep_for(std::chrono::milliseconds(ticks)); }

inline BaseType_t xTaskCreatePinnedToCore(void (*function)(void*), const char* /*name*/, uint32_t /*stackSize*/,
                                          void* parameters, UBaseType_t /*priority*/, TaskHandle_t* /*handle*/,
                                          BaseType_t /*core*/) {
  std::thread(function, parameters).detach();
  return pdPASS;
}

#endif  // JL_SHIM_FREERTOS_TASK_H
