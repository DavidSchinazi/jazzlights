#ifndef JL_UI_DISABLED_H
#define JL_UI_DISABLED_H

#include "jazzlights/ui/ui.h"

#ifdef ESP32

namespace jazzlights {

class NoOpUi : public Esp32Ui {
 public:
  explicit NoOpUi(Player& player, Milliseconds currentTime) : Esp32Ui(player, currentTime) {}
  void InitialSetup(Milliseconds currentTime) override;
  void FinalSetup(Milliseconds /*currentTime*/) override {}
  void RunLoop(Milliseconds /*currentTime*/) override {}
};

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_UI_DISABLED_H
