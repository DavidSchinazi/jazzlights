#include "jazzlights/effect/creatures.h"

#if JL_IS_CONFIG(CREATURE)

#include <esp_mac.h>

namespace jazzlights {

namespace {

constexpr Milliseconds kRssiDecayDelayMs = 3000;  // Decay after 3s of inactivity.
constexpr int kRssiDecayFactor = 5;               // Decay RSSI by 5dBm/s after delay.
// Consider creatures far away if no nearby RSSI within 3 second.
constexpr Milliseconds kNearbyCreatureTimeoutMs = 3000;

constexpr int kRssiFloor = -90;
constexpr int kRssiCeiling = -40;
constexpr int kRssiMax = 20;
static_assert(kRssiMax > kRssiCeiling, "unexpected RSSI");
constexpr int kRssiNearbyThresholdUp = -69;
constexpr int kRssiNearbyThresholdDown = -75;
constexpr uint32_t kMinCreaturesForAParty = 3;  // Minimum number of creatures to trigger a party.

uint32_t ColorHash(uint32_t color) {
  // Allows sorting colors in a way that isn't trivially predictable by humans.
  color ^= color << 13;
  color ^= color >> 17;
  color ^= color << 5;
  return color;
}

uint8_t CreatureIntensity(const Creature& creature) {
  int rssi = creature.smoothedRssi;
  // According to Espressif documentation, their API provides RSSI in dBm with values between -127 and +20.
  // In practice, the sensors generally aren't able to pick up anything below -100, and rarely below -90.
  // When two ATOM Matrix devices are right next to each other, the highest observed value was -26 but we almost never
  // see anything above -40.
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
    // Pick a color from kColors but then use the MAC address hash to randomize the lower bits.
    static constexpr CRGB kColors[] = {
        CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Purple, CRGB::Cyan,
    };
    size_t colorIndex = x % (sizeof(kColors) / sizeof(kColors[0]));
    uint32_t color = static_cast<uint32_t>(kColors[colorIndex]);
    color &= 0xf0f0f0;
    color |= (x & 0x0f0f0f);
    return color;
  }();
  return sThisCreatureColor;
#endif  // JL_CREATURE_COLOR
}

KnownCreatures* KnownCreatures::Get() {
  static KnownCreatures sKnownCreatures;
  return &sKnownCreatures;
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

void KnownCreatures::AddCreature(uint32_t color, Milliseconds lastHeard, int rssi, bool isPartying) {
  bool found = false;
  for (Creature& creature : creatures_) {
    if (creature.color == color) {
      if (lastHeard > creature.lastHeard) {
        creature.lastHeard = lastHeard;
        creature.smoothedRssi += (rssi - creature.smoothedRssi) / 2;
        if (isPartying) {
          creature.lastHeardPartying = lastHeard;
        } else {
          creature.lastHeardPartying = -1;
        }
      }
      found = true;
      break;
    }
  }
  if (!found) {
    Creature new_creature;
    new_creature.color = color;
    new_creature.lastHeard = lastHeard;
    if (isPartying) {
      new_creature.lastHeardPartying = lastHeard;
    } else {
      new_creature.lastHeardPartying = -1;
    }
    new_creature.smoothedRssi = rssi;
    new_creature.isNearby = rssi >= kRssiNearbyThresholdUp;
    creatures_.push_back(new_creature);
    jll_info("%u Adding new creature rgb=%06x rssi=%d %s %s partying", timeMillis(), static_cast<int>(color), rssi,
             (new_creature.isNearby ? "near" : "far"), (isPartying ? "is" : "not"));
  }
  std::sort(creatures_.begin(), creatures_.end(),
            [](Creature a, Creature b) { return ColorHash(a.color) > ColorHash(b.color); });
}

void KnownCreatures::update() {
  Milliseconds currentTime = timeMillis();
  for (Creature& creature : creatures_) {
    int decayedRssi = creature.smoothedRssi;
    if (creature.lastHeard >= 0 && currentTime - creature.lastHeard > kRssiDecayDelayMs) {
      // If we haven't heard from this creature in kRssiDecayDelayMs, decay the RSSI by kRssiDecayFactor dBm every
      // second.
      decayedRssi = creature.smoothedRssi -
                    (kRssiDecayFactor * (currentTime - creature.lastHeard - kRssiDecayDelayMs)) / ONE_SECOND;
    }
    if (decayedRssi >= kRssiNearbyThresholdUp && !creature.isNearby) {
      jll_info("%u Adding nearby to creature rgb=%06x rssi=%d", currentTime, static_cast<int>(creature.color),
               decayedRssi);
      creature.isNearby = true;
    } else if (decayedRssi <= kRssiNearbyThresholdDown && creature.isNearby) {
      jll_info("%u Removing nearby from creature rgb=%06x rssi=%d", currentTime, static_cast<int>(creature.color),
               decayedRssi);
      creature.isNearby = false;
    }
  }
}

KnownCreatures::KnownCreatures() {
  Creature ourselves;
  ourselves.color = ThisCreatureColor();
  jll_info("%u Using color rgb=%06x for ourselves", timeMillis(), static_cast<int>(ourselves.color));
  ourselves.lastHeard = -1;
  ourselves.lastHeardPartying = -1;
  ourselves.smoothedRssi = kRssiMax;
  ourselves.isNearby = true;
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
  KnownCreatures::Get()->update();
  const std::vector<Creature>& creatures = KnownCreatures::Get()->creatures();
  size_t num_creatures = creatures.size();
  if (num_creatures > kMaxNumCreatureColours - 2) { num_creatures = kMaxNumCreatureColours - 2; }
  uint32_t num_close_creatures = 0;
  bool iShouldParty = false;
  Milliseconds currentTime = timeMillis();
  for (size_t i = 0; i < num_creatures; i++) {
    // Compute the color for each known creature.
    const Creature& creature = creatures[i];
    uint8_t intensity = CreatureIntensity(creature);
    state(frame)->colors[i] = FadeColor(CRGB(creature.color), intensity);
    if (creature.isNearby) { num_close_creatures++; }
    if (creature.lastHeardPartying >= 0 && currentTime - creature.lastHeardPartying < kNearbyCreatureTimeoutMs) {
      iShouldParty = true;
    }
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
  KnownCreatures::Get()->SetIsPartying(num_close_creatures >= kMinCreaturesForAParty);
  state(frame)->rainbow = iShouldParty || KnownCreatures::Get()->IsPartying();
  state(frame)->initialHue = 256 * frame.time / 1667;
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
