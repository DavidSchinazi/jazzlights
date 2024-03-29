#ifndef JL_UI_CORE2_H
#define JL_UI_CORE2_H

#include "jazzlights/ui/ui.h"

#ifdef ARDUINO
#if JL_IS_CONTROLLER(CORE2AWS)

#ifndef CORE2AWS_LCD_ENABLED
#define CORE2AWS_LCD_ENABLED 1
#endif  // CORE2AWS_LCD_ENABLED

#include "jazzlights/ui/ui_disabled.h"

namespace jazzlights {

#if CORE2AWS_LCD_ENABLED

class Core2AwsUi : public ArduinoUi {
 public:
  explicit Core2AwsUi(Player& player, Milliseconds currentTime);
  // From ArduinoUi.
  void InitialSetup(Milliseconds currentTime) override;
  void FinalSetup(Milliseconds currentTime) override;
  void RunLoop(Milliseconds currentTime) override;

 private:
  void DrawSystemTextLines(Milliseconds currentTime);

  uint8_t ledBrightness_;
  uint8_t onBrightness_;
};

#else   // CORE2AWS_LCD_ENABLED
typedef NoOpUi Core2AwsUi;
#endif  // CORE2AWS_LCD_ENABLED

}  // namespace jazzlights

#endif  // JL_IS_CONTROLLER(CORE2AWS)
#endif  // ARDUINO
#endif  // JL_UI_CORE2_H