#ifndef JL_UI_UI_H
#define JL_UI_UI_H

#include "jazzlights/config.h"

#ifdef ESP32

#ifndef JL_BUTTON_LOCK
#define JL_BUTTON_LOCK (!JL_DEV)
#endif  // JL_BUTTON_LOCK

#include "jazzlights/fastled_runner.h"
#include "jazzlights/player.h"

namespace jazzlights {

class Esp32Ui {
 public:
  explicit Esp32Ui(Player& player) : player_(player) {}
  virtual ~Esp32Ui() = default;

  virtual void set_fastled_runner(FastLedRunner* runner) { (void)runner; }

  virtual void InitialSetup() = 0;
  virtual void FinalSetup() = 0;
  virtual void RunLoop(Milliseconds currentTime) = 0;

 protected:
  Player& player_;
};

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_UI_UI_H
