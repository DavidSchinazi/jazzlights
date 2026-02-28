#ifndef JL_UI_TOUCH_BUTTON_H
#define JL_UI_TOUCH_BUTTON_H

#include "jazzlights/config.h"

#ifndef JL_TOUCH_SCREEN
#if defined(ESP32) && (JL_IS_CONTROLLER(CORE2AWS) || JL_IS_CONTROLLER(CORES3))
#define JL_TOUCH_SCREEN 1
#else  // CORE2AWS || CORES3
#define JL_TOUCH_SCREEN 0
#endif  // CORE2AWS || CORES3
#endif  // JL_TOUCH_SCREEN

#if JL_TOUCH_SCREEN

#include <M5Unified.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace jazzlights {

class TouchButton {
 public:
  // Modify text inside the button.
  void SetLabelText(const char* label);
  void SetLabelDatum(int16_t x_delta, int16_t y_delta,
                     lgfx::textdatum::textdatum_t datum = lgfx::textdatum::textdatum_t::middle_center);

  // `Draw()` unhides a button and paints it if needed. `force` ensures that paint always happens.
  void Draw(bool force = false);
  void Hide();
  // A highlighted button will have a white background instead of black.
  void SetHighlight(bool highlight) { highlight_ = highlight; }

  // Queries the latest button state.
  bool IsPressed() const;
  bool JustPressed() const;
  bool JustReleased() const;

  using PaintFunction = std::function<void(TouchButton* button, int outline, int fill, int textColor)>;
  void SetCustomPaintFunction(PaintFunction customPaintFunction) { customPaintFunction_ = customPaintFunction; }
  // `PaintRectangle()` and `PaintText()` are used internally and also exposed publicly so they can be called from
  // custom paint functions.
  void PaintRectangle(int fill, int outline);
  void PaintText(int textColor, int fill, const char* label = nullptr);

  // The functions below should only be called by TouchButtonManager. Buttons are created using
  // `TouchButtonManager::AddButton()`.
  void Setup(int16_t x, int16_t y, uint16_t w, uint16_t h, const char* label);
  bool HandlePress(int16_t x, int16_t y);
  void HandleIdle();
  void Paint();
  void MaybePaint(bool force = false);
  bool IsHidden() const { return hidden_; }

 private:
  enum class DrawState {
    kNotDrawn,
    kHidden,
    kIdle,
    kPressed,
  };
  LGFX_Button lgfxButton_;
  std::string label_;
  bool hidden_ = true;
  bool highlight_ = false;
  int16_t x_ = 0;
  int16_t y_ = 0;
  uint16_t w_ = 0;
  uint16_t h_ = 0;
  int16_t dx_ = 0;
  int16_t dy_ = 0;
  DrawState lastDrawState_ = DrawState::kNotDrawn;
  PaintFunction customPaintFunction_;
};

class TouchButtonManager {
 public:
  static TouchButtonManager* Get();

  TouchButton* AddButton(int16_t x, int16_t y, uint16_t w, uint16_t h, const char* label);

  bool HandlePress(int16_t x, int16_t y);
  void HandleIdle();
  void MaybePaint();

  void RedrawRightHalf();
  void Redraw();

 private:
  TouchButtonManager() = default;
  std::vector<std::unique_ptr<TouchButton>> buttons_;
};

}  // namespace jazzlights

#endif  // JL_TOUCH_SCREEN
#endif  // JL_UI_TOUCH_BUTTON_H
