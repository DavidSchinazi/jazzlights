#include "jazzlights/effect/creatures.h"

#if JL_IS_CONFIG(CREATURE)

#include <esp_mac.h>

namespace jazzlights {

namespace {

uint32_t ColorHash(uint32_t color) {
  // Allows sorting colors in a way that isn't trivially predictable by humans.
  color ^= color << 13;
  color ^= color >> 17;
  color ^= color << 5;
  return color;
}

uint8_t CreatureIntensity(const Creature& creature, Milliseconds currentTime) {
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

}  // namespace

uint32_t ThisCreatureColor() {
#ifdef JL_CREATURE_COLOR
  return JL_CREATURE_COLOR;
#else  // JL_CREATURE_COLOR
  static const uint32_t sThisCreatureColor = []() {
    uint64_t x = 0;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    ESP_ERROR_CHECK(esp_read_mac(reinterpret_cast<uint8_t*>(&x), ESP_MAC_EFUSE_FACTORY));
#else
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(reinterpret_cast<uint8_t*>(&x)));
#endif
    for (int i = 0; i < 8; i++) {
      // Do a few rounds of xorshift* to turn the MAC address into something pretty uniformaly distributed, and also
      // swap some bytes because, who knows, that might help?
      x = ((x & 0x0000FFFFFFFFFFFF) << 16) | ((x & 0xFFFF000000000000) >> 48);
      x ^= x >> 12;
      x ^= x << 25;
      x ^= x >> 27;
      x *= 0x2545F4914F6CDD1DULL;
    }
    static constexpr CRGB kColors[] = {
        CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Purple, CRGB::Cyan,
    };
    x %= (sizeof(kColors) / sizeof(kColors[0]));
    return static_cast<uint32_t>(kColors[x]);
  }();
  return sThisCreatureColor;
#endif  // JL_CREATURE_COLOR
}

void KnownCreatures::ExpireOldEntries(Milliseconds currentTime) {
  creatures_.erase(std::remove_if(creatures_.begin(), creatures_.end(),
                                  [currentTime](const Creature& creature) {
                                    static constexpr Milliseconds kCreatureExpirationTime = 10000;  // 10s.
                                    return creature.lastHeard >= 0 &&
                                           currentTime - creature.lastHeard > kCreatureExpirationTime;
                                  }),
                   creatures_.end());
}

void KnownCreatures::AddCreature(uint32_t color, Milliseconds lastHeard, int rssi) {
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

KnownCreatures::KnownCreatures() {
  Creature ourselves;
  ourselves.color = ThisCreatureColor();
  ourselves.lastHeard = -1;
  ourselves.rssi = 20;
  creatures_.push_back(ourselves);
}

// Called once to initialize the state.
void Creatures::begin(const Frame& frame) const {
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

// Called once for each point in time to prepare the state before calling color() for each pixel.
void Creatures::rewind(const Frame& frame) const {
  const Milliseconds currentTime = timeMillis();
  const std::vector<Creature>& creatures = KnownCreatures::Get()->creatures();
  size_t num_creatures = creatures.size();
  if (num_creatures > kMaxNumColors - 2) { num_creatures = kMaxNumColors - 2; }
  uint32_t num_close_creatures = 0;
  for (size_t i = 0; i < num_creatures; i++) {
    // Compute the color for each known creature.
    const Creature& creature = creatures[i];
    uint8_t intensity = CreatureIntensity(creature, currentTime);
    state(frame)->colors[i] = FadeColor(CRGB(creature.color), intensity);
    if (intensity >= 100) { num_close_creatures++; }
  }
  state(frame)->colors[num_creatures] = CRGB::Black;
  state(frame)->colors[num_creatures + 1] = CRGB::Black;
  state(frame)->num_colors = num_creatures + 2;
  // The offset increases over time and makes the pixels scroll.
  state(frame)->offset = frame.time / 100;
  // TODO the rainbow mode sometimes only triggers on some creatures but not others. To fix it, we need to make the
  // metric bidirectional. We can do that by having every node broadcast its list of known creatures and decayed RSSI
  // (or intensity) then we use a symmetric function (such as min or average) to decide when to put it in
  // num_close_creatures.
  state(frame)->rainbow = num_close_creatures >= 3;
  state(frame)->initialHue = 256 * frame.time / 1500;
}

// Called for each pixel to compute its color.
CRGB Creatures::color(const Frame& frame, const Pixel& px) const {
  if (state(frame)->rainbow) {
    // The rainbow effect determines the hue based on the distance from the rainbow origin point.
    const double d = distance(px.coord, state(frame)->origin);
    return ColorFromPalette(RainbowColors_p,
                            state(frame)->initialHue + int32_t(255 * d / state(frame)->maxDistance) % 255);
  }
  size_t num_colors = state(frame)->num_colors;
  // Display the colors in order based on the current offset.
  const size_t reverseIndex = (-px.index % num_colors) + num_colors - 1;
  return state(frame)->colors[(state(frame)->offset + reverseIndex) % num_colors];
}

// Called once for each point in time after all pixels have been computed.
void Creatures::afterColors(const Frame& /*frame*/) const {
  static_assert(std::is_trivially_destructible<CreaturesState>::value, "CreaturesState must be trivially destructible");
}

}  // namespace jazzlights

#endif  // CREATURE
