#ifndef JL_UI_ATOM_S3_H
#define JL_UI_ATOM_S3_H

#include "jazzlights/ui/ui.h"

#ifdef ARDUINO

namespace jazzlights {

class AtomS3Ui : public ArduinoUi {
 public:
  explicit AtomS3Ui(Player& player, Milliseconds currentTime);
  void InitialSetup(Milliseconds currentTime) override;
  void FinalSetup(Milliseconds currentTime) override;
  void RunLoop(Milliseconds currentTime) override;
};

}  // namespace jazzlights

#endif  // ARDUINO
#endif  // JL_UI_ATOM_S3_H
