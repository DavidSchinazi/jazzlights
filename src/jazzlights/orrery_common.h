#ifndef JL_ORRERY_COMMON_H
#define JL_ORRERY_COMMON_H

#include <cstdint>

namespace jazzlights {

enum class Planet : uint8_t {
  Mercury = 4,
  Venus = 5,
  Earth = 6,
  Mars = 7,
  Jupiter = 8,
  Saturn = 9,
  Uranus = 10,
  Neptune = 11,
  NumPlanets = 12,
};

enum class OrreryMessageType : uint8_t {
  SetSpeed = 0x01,
  AckSpeed = 0x02,
};

struct OrreryMessage {
  OrreryMessageType type;
  uint8_t planetIndex;
  int32_t speed;
} __attribute__((packed));

}  // namespace jazzlights

#endif  // JL_ORRERY_COMMON_H
