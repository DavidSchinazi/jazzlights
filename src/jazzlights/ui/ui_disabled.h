#ifndef JL_UI_UI_DISABLED_H
#define JL_UI_UI_DISABLED_H

#include "jazzlights/ui/ui.h"

#ifdef ESP32

namespace jazzlights {

class NoOpUi : public Esp32Ui {
 public:
  explicit NoOpUi(Player& player) : Esp32Ui(player) {}
  void InitialSetup() override;
  void FinalSetup() override {}
  void RunLoop(Milliseconds /*currentTime*/) override {}
};

}  // namespace jazzlights

#endif  // ESP32
#endif  // JL_UI_UI_DISABLED_H
