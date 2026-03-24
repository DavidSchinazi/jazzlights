#ifndef JL_ORRERY_COMMON_H
#define JL_ORRERY_COMMON_H

#include <cstdint>

namespace jazzlights {

enum class Planet : uint8_t { Mercury = 0, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune, NumPlanets };

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
