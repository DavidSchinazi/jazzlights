#ifndef JL_EFFECT_CREATURES_H
#define JL_EFFECT_CREATURES_H

#include <algorithm>

#include "jazzlights/config.h"
#include "jazzlights/effect/effect.h"
#include "jazzlights/util/math.h"

#if JL_IS_CONFIG(CREATURE)

#ifndef JL_CREATURE_COLOR
#define JL_CREATURE_COLOR 0xFFFFFF
#endif  // JL_CREATURE_COLOR

namespace jazzlights {

constexpr uint32_t ThisCreatureColor() { return JL_CREATURE_COLOR; }

inline uint32_t ColorHash(uint32_t color) {
  // Allows sorting colors in a way that isn't trivially predictable by humans.
  color ^= color << 13;
  color ^= color >> 17;
  color ^= color << 5;
  return color;
}

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
  void ExpireOldEntries(Milliseconds currentTime) {
    creatures_.erase(std::remove_if(creatures_.begin(), creatures_.end(),
                                    [currentTime](const Creature& creature) {
                                      static constexpr Milliseconds kCreatureExpirationTime = 10000;  // 10s.
                                      return creature.lastHeard >= 0 &&
                                             currentTime - creature.lastHeard > kCreatureExpirationTime;
                                    }),
                     creatures_.end());
  }
  void AddCreature(uint32_t color, Milliseconds lastHeard, int rssi) {
    bool found = false;
    for (Creature& creature : creatures_) {
      if (creature.color == color) {
        if (lastHeard > creature.lastHeard) {
          creature.lastHeard = lastHeard;
          creature.rssi = rssi;
        }
        found = true;
        break;
      }
    }
    if (!found) {
      Creature new_creature;
      new_creature.color = color;
      new_creature.lastHeard = lastHeard;
      new_creature.rssi = rssi;
      creatures_.push_back(new_creature);
    }
    std::sort(creatures_.begin(), creatures_.end(),
              [](Creature a, Creature b) { return ColorHash(a.color) > ColorHash(b.color); });
  }

 private:
  KnownCreatures() {
    Creature ourselves;
    ourselves.color = ThisCreatureColor();
    ourselves.lastHeard = -1;
    ourselves.rssi = 20;
    creatures_.push_back(ourselves);
  }
  std::vector<Creature> creatures_;
};

class Creatures : public Effect {
 public:
  Creatures() = default;

  std::string effectName(PatternBits /*pattern*/) const override { return "creatures"; }

  size_t contextSize(const Frame& /*frame*/) const override { return sizeof(CreaturesState); }

  void begin(const Frame& frame) const override {
    new (state(frame)) CreaturesState;  // Default-initialize the state.
  }

  void rewind(const Frame& frame) const override {
    const std::vector<Creature>& creatures = KnownCreatures::Get()->creatures();
    size_t num_creatures = creatures.size();
    Milliseconds period_duration = PeriodDuration(num_creatures);
    size_t num_period = (frame.time / period_duration) % num_creatures;
    if (num_period > creatures.size()) { jll_fatal("bad creature math"); }
    const Creature& creature = creatures[num_period];
    uint8_t intensity = RssiToIntensity(creature.rssi);
    state(frame)->color = FadeColor(CRGB(creature.color), intensity);
  }

  void afterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<CreaturesState>::value,
                  "CreaturesState must be trivially destructible");
  }

  CRGB color(const Frame& frame, const Pixel& px) const override { return state(frame)->color; }

 private:
  struct CreaturesState {
    CRGB color;
  };
  CreaturesState* state(const Frame& frame) const {
    static_assert(alignof(CreaturesState) <= kMaxStateAlignment, "Need to increase kMaxStateAlignment");
    return static_cast<CreaturesState*>(frame.context);
  }
  static Milliseconds PeriodDuration(size_t num_creatures) {
    // The return value needs to (almost) evenly divide 10s to the repeat isn't visible.
    switch (num_creatures) {
      case 1: return 1000;
      case 2: return 1000;
      case 3: return 1111;
      case 4: return 1250;
      case 5: return 1000;
      default: return 10000 / num_creatures;
    }
  }
  static uint8_t RssiToIntensity(int rssi) {
    // According to Espressif documentation, their API provides RSSI in dBm with values between -127 and +20.
    // In practice, the sensors generally aren't able to pick up anything below -100, and rarely below -90.
    // When two ATOM Matrix devices are right next to each other, the highest observed value was -26 but we almost never
    // see anything above -40.
    static constexpr int kRssiFloor = -80;
    static constexpr int kRssiCeiling = -40;
    if (rssi <= kRssiFloor) { return 0; }
    if (rssi >= kRssiCeiling) { return 255; }
    return static_cast<uint8_t>((static_cast<double>(rssi - kRssiFloor) * 255.0) /
                                static_cast<double>(kRssiCeiling - kRssiFloor));
  }
  static uint8_t FadeSubColor(uint8_t channel, uint8_t intensity) {
    if (intensity == 255) { return channel; }
    if (channel == 255) { return intensity; }
    if (channel == 0 || intensity == 0) { return 0; }
    return static_cast<uint8_t>(static_cast<double>(channel) * static_cast<double>(intensity) / 255.0);
  }
  static CRGB FadeColor(CRGB color, uint8_t intensity) {
    return CRGB(FadeSubColor(color.red, intensity), FadeSubColor(color.green, intensity),
                FadeSubColor(color.blue, intensity));
  }
};

}  // namespace jazzlights
#endif  // CREATURE
#endif  // JL_EFFECT_CREATURES_H
