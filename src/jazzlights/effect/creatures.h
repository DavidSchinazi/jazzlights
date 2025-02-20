#ifndef JL_EFFECT_CREATURES_H
#define JL_EFFECT_CREATURES_H

#include <algorithm>

#include "jazzlights/config.h"
#include "jazzlights/effect/effect.h"
#include "jazzlights/util/math.h"

#if JL_IS_CONFIG(CREATURE)

namespace jazzlights {

uint32_t ThisCreatureColor();

struct Creature {
  uint32_t color;
  Milliseconds lastHeard;
  int rssi;
};

class KnownCreatures {
 public:
  // Disallow copy and move.
  KnownCreatures(const KnownCreatures&) = delete;
  KnownCreatures(KnownCreatures&&) = delete;
  KnownCreatures& operator=(const KnownCreatures&) = delete;
  KnownCreatures& operator=(KnownCreatures&&) = delete;
  // Singleton. Can only be accessed from main runloop (so from Player or from Creatures effect).
  static KnownCreatures* Get();
  const std::vector<Creature>& creatures() const { return creatures_; }
  void ExpireOldEntries(Milliseconds currentTime);
  void AddCreature(uint32_t color, Milliseconds lastHeard, int rssi);

 private:
  KnownCreatures();
  std::vector<Creature> creatures_;
};

class Creatures : public Effect {
 public:
  Creatures() = default;

  std::string effectName(PatternBits /*pattern*/) const override { return "creatures"; }

  size_t contextSize(const Frame& /*frame*/) const override { return sizeof(CreaturesState); }

  void begin(const Frame& frame) const override;
  void rewind(const Frame& frame) const override;
  void afterColors(const Frame& /*frame*/) const override;
  CRGB color(const Frame& frame, const Pixel& px) const override;

 private:
  static constexpr size_t kMaxNumColors = 256;
  struct CreaturesState {
    Point origin;
    double maxDistance;
    size_t num_colors;
    size_t offset;
    bool rainbow;
    uint8_t initialHue;
    CRGB colors[kMaxNumColors];
  };
  CreaturesState* state(const Frame& frame) const {
    static_assert(alignof(CreaturesState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<CreaturesState*>(frame.context);
  }
};

}  // namespace jazzlights
#endif  // CREATURE
#endif  // JL_EFFECT_CREATURES_H
