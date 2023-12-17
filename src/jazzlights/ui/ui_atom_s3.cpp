#include "jazzlights/ui/ui_atom_s3.h"

#ifdef ARDUINO
#if JL_IS_CONTROLLER(ATOM_S3)

#include <M5Unified.h>

namespace jazzlights {
namespace {
static constexpr uint8_t kButtonPin = 41;

static constexpr uint8_t kBrightnessList[] = {2, 4, 8, 16, 32, 64, 128, 255};
static constexpr uint8_t kNumBrightnesses = sizeof(kBrightnessList) / sizeof(kBrightnessList[0]);

#if JL_DEV
static constexpr uint8_t kInitialBrightnessCursor = 0;
#elif JL_IS_CONFIG(STAFF)
static constexpr uint8_t kInitialBrightnessCursor = 3;
#elif JL_IS_CONFIG(HAMMER)
static constexpr uint8_t kInitialBrightnessCursor = 7;
#else
static constexpr uint8_t kInitialBrightnessCursor = 4;
#endif
}  // namespace

AtomS3Ui::AtomS3Ui(Player& player, Milliseconds currentTime)
    : ArduinoUi(player, currentTime),
      button_(kButtonPin, *this, currentTime),
      brightnessCursor_(kInitialBrightnessCursor) {}

void AtomS3Ui::InitialSetup(Milliseconds currentTime) {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.init();
}

void AtomS3Ui::FinalSetup(Milliseconds currentTime) { UpdateScreen(currentTime); }

void AtomS3Ui::RunLoop(Milliseconds currentTime) {
  button_.RunLoop(currentTime);
  M5.update();
}

void AtomS3Ui::ShortPress(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u ShortPress", currentTime);

  // Act on current menu mode.
  switch (menuMode_) {
    case MenuMode::kNext:
      jll_info("%u Next button has been hit", currentTime);
      player_.stopSpecial(currentTime);
      player_.stopLooping(currentTime);
      player_.next(currentTime);
      break;
    case MenuMode::kPrevious:
      jll_info("%u Back button has been hit", currentTime);
      player_.stopSpecial(currentTime);
      player_.loopOne(currentTime);
      break;
    case MenuMode::kBrightness:
      brightnessCursor_ = (brightnessCursor_ + 1 < kNumBrightnesses) ? brightnessCursor_ + 1 : 0;
      jll_info("%u Brightness button has been hit %u", currentTime, kBrightnessList[brightnessCursor_]);
      player_.SetBrightness(kBrightnessList[brightnessCursor_]);
      break;
    case MenuMode::kSpecial:
      jll_info("%u Special button has been hit", currentTime);
      player_.handleSpecial(currentTime);
      break;
  }
}

void AtomS3Ui::LongPress(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u LongPress", currentTime);

  // Move to next menu mode.
  switch (menuMode_) {
    case MenuMode::kNext: menuMode_ = MenuMode::kPrevious; break;
    case MenuMode::kPrevious: menuMode_ = MenuMode::kBrightness; break;
    case MenuMode::kBrightness: menuMode_ = MenuMode::kNext; break;
    case MenuMode::kSpecial: menuMode_ = MenuMode::kNext; break;
  }
  UpdateScreen(currentTime);
}

void AtomS3Ui::HeldDown(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u HeldDown", currentTime);
  menuMode_ = MenuMode::kSpecial;
  M5.Display.clearDisplay(::PURPLE);
}

void AtomS3Ui::UpdateScreen(Milliseconds currentTime) {
  switch (menuMode_) {
    case MenuMode::kNext: {
      M5.Display.clearDisplay(::BLUE);
    } break;
    case MenuMode::kPrevious: {
      M5.Display.clearDisplay(::RED);
    } break;
    case MenuMode::kBrightness: {
      M5.Display.clearDisplay(::YELLOW);
    } break;
    case MenuMode::kSpecial: {
      M5.Display.clearDisplay(::PURPLE);
    } break;
  }
}

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_S3)
#endif  // ARDUINO
