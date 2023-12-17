#ifndef JL_UI_DISABLED_H
#define JL_UI_DISABLED_H

#include "jazzlights/ui/ui.h"

#ifdef ARDUINO

namespace jazzlights {

class NoOpUi : public ArduinoUi {
 public:
  explicit NoOpUi(Player& player, Milliseconds currentTime) : ArduinoUi(player, currentTime) {}
  void InitialSetup(Milliseconds currentTime) override;
  void FinalSetup(Milliseconds /*currentTime*/) override {}
  void RunLoop(Milliseconds /*currentTime*/) override {}
};

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_UI_DISABLED_H
