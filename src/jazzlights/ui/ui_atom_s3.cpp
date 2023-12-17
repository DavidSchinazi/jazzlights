#include "jazzlights/ui/ui_atom_s3.h"

namespace jazzlights {

AtomS3Ui::AtomS3Ui(Player& player, Milliseconds currentTime) : ArduinoUi(player, currentTime) {}

void AtomS3Ui::InitialSetup(Milliseconds /*currentTime*/) {}
void AtomS3Ui::FinalSetup(Milliseconds /*currentTime*/) {}
void AtomS3Ui::RunLoop(Milliseconds /*currentTime*/) {}

}  // namespace jazzlights
