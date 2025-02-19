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
    state(frame)->origin = {
        frame.viewport.origin.x + frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.width),
        frame.viewport.origin.y + frame.predictableRandom->GetRandomDoubleBetween(0, frame.viewport.size.height),
    };
    state(frame)->maxDistance = std::max({
        distance(state(frame)->origin, lefttop(frame)),
        distance(state(frame)->origin, righttop(frame)),
        distance(state(frame)->origin, leftbottom(frame)),
        distance(state(frame)->origin, rightbottom(frame)),
    });
  }

  void rewind(const Frame& frame) const override {
    const Milliseconds currentTime = timeMillis();
    const std::vector<Creature>& creatures = KnownCreatures::Get()->creatures();
    size_t num_creatures = creatures.size();
    if (num_creatures > kMaxNumColors - 2) { num_creatures = kMaxNumColors - 2; }
    uint32_t num_close_creatures = 0;
    for (size_t i = 0; i < num_creatures; i++) {
      const Creature& creature = creatures[i];
      uint8_t intensity = CreatureIntensity(creature, currentTime);
      state(frame)->colors[i] = FadeColor(CRGB(creature.color), intensity);
      if (intensity >= 100) { num_close_creatures++; }
    }
    state(frame)->colors[num_creatures] = CRGB::Black;
    state(frame)->colors[num_creatures + 1] = CRGB::Black;
    state(frame)->num_colors = num_creatures + 2;
    state(frame)->offset = frame.time / 100;
    state(frame)->rainbow = num_close_creatures >= 3;
    state(frame)->initialHue = 256 * frame.time / 1500;
  }

  void afterColors(const Frame& /*frame*/) const override {
    static_assert(std::is_trivially_destructible<CreaturesState>::value,
                  "CreaturesState must be trivially destructible");
  }

  CRGB color(const Frame& frame, const Pixel& px) const override {
    if (state(frame)->rainbow) {
      const double d = distance(px.coord, state(frame)->origin);
      return ColorFromPalette(RainbowColors_p,
                              state(frame)->initialHue + int32_t(255 * d / state(frame)->maxDistance) % 255);
    }
    size_t num_colors = state(frame)->num_colors;
    const size_t reverseIndex = (-px.index % num_colors) + num_colors - 1;
    return state(frame)->colors[(state(frame)->offset + reverseIndex) % num_colors];
  }

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
  static uint8_t CreatureIntensity(const Creature& creature, Milliseconds currentTime) {
    int rssi = creature.rssi;
    if (creature.lastHeard >= 0) {
      Milliseconds timeSinceHeard = currentTime - creature.lastHeard;
      if (timeSinceHeard >= 3000) {
        // If we haven't heard from this creature in 3s, decay the RSSI by 5 dBm every second.
        rssi -= (timeSinceHeard - 3000) / 200;
      }
    }
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
};

}  // namespace jazzlights
#endif  // CREATURE
#endif  // JL_EFFECT_CREATURES_H
