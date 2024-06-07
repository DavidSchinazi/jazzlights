#ifndef JL_UI_H
#define JL_UI_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#ifndef JL_BUTTON_LOCK
#define JL_BUTTON_LOCK (!JL_DEV)
#endif  // JL_BUTTON_LOCK

#include "jazzlights/fastled_runner.h"
#include "jazzlights/player.h"

namespace jazzlights {

class ArduinoUi {
 public:
  explicit ArduinoUi(Player& player, Milliseconds currentTime) : player_(player) { (void)currentTime; }
  virtual ~ArduinoUi() = default;

  virtual void set_fastled_runner(FastLedRunner* runner) { (void)runner; }

  virtual void InitialSetup(Milliseconds currentTime) = 0;
  virtual void FinalSetup(Milliseconds currentTime) = 0;
  virtual void RunLoop(Milliseconds currentTime) = 0;

 protected:
  Player& player_;
};

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_UI_H
