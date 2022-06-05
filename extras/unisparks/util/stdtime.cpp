#include "unisparks/util/time.hpp"
#include <chrono>
#include <thread>
namespace unisparks {

Milliseconds timeMillis() {
  static auto t0 = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - t0)
         .count();
}

} // namespace unisparks



