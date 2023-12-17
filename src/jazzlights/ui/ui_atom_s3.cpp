#include "jazzlights/ui/ui_atom_s3.h"

#ifdef ARDUINO
#if JL_IS_CONTROLLER(ATOM_S3)

#include <M5Unified.h>

namespace jazzlights {
namespace {
static constexpr uint8_t kButtonPin = 41;
}  // namespace

AtomS3Ui::AtomS3Ui(Player& player, Milliseconds currentTime)
    : ArduinoUi(player, currentTime), button_(kButtonPin, *this, currentTime) {}

void AtomS3Ui::InitialSetup(Milliseconds currentTime) {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.init();
  M5.Display.startWrite();
  M5.Display.clearDisplay();
  M5.Display.endWrite();
}
void AtomS3Ui::FinalSetup(Milliseconds currentTime) {}
void AtomS3Ui::RunLoop(Milliseconds currentTime) {
  button_.RunLoop(currentTime);
  M5.update();
}

void AtomS3Ui::ShortPress(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u ShortPress", currentTime);
  foo_ = !foo_;
  if (foo_) {
    M5.Display.startWrite();
    M5.Display.clearDisplay(::GREEN);
    M5.Display.endWrite();
  } else {
    M5.Display.startWrite();
    M5.Display.clearDisplay(::RED);
    M5.Display.endWrite();
  }
}

void AtomS3Ui::LongPress(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u LongPress", currentTime);
}

void AtomS3Ui::HeldDown(uint8_t pin, Milliseconds currentTime) {
  if (pin != kButtonPin) { return; }
  jll_info("%u HeldDown", currentTime);
}

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(ATOM_S3)
#endif  // ARDUINO
