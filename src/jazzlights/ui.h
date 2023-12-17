#ifndef JL_UI_H
#define JL_UI_H

#include "jazzlights/config.h"

#ifdef ARDUINO

#ifndef JL_BUTTON_LOCK
#define JL_BUTTON_LOCK (!JL_DEV)
#endif  // JL_BUTTON_LOCK

#include "jazzlights/player.h"

namespace jazzlights {

class ArduinoUi {
 public:
  explicit ArduinoUi(Player& player, Milliseconds currentTime) : player_(player) { (void)currentTime; }
  virtual ~ArduinoUi() = default;

  virtual void InitialSetup(Milliseconds currentTime) = 0;
  virtual void FinalSetup(Milliseconds currentTime) = 0;
  virtual void RunLoop(Milliseconds currentMillis) = 0;

 protected:
  Player& player_;
};

class NoOpUi : public ArduinoUi {
 public:
  explicit NoOpUi(Player& player, Milliseconds currentTime) : ArduinoUi(player, currentTime) {}
  void InitialSetup(Milliseconds /*currentTime*/) override {}
  void FinalSetup(Milliseconds /*currentTime*/) override {}
  void RunLoop(Milliseconds /*currentMillis*/) override {}
};

uint8_t getBrightness();

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_UI_H
