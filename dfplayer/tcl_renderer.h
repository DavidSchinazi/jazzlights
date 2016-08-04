// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//
// TCL controller access module.

#ifndef __DFPLAYER_TCL_RENDERER_H
#define __DFPLAYER_TCL_RENDERER_H

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "tcl/tcl_types.h"
#include "utils.h"
#include "util/led_layout.h"
#include "util/pixels.h"

class Effect;
class FishifyEffect;
class PassthroughEffect;
class RainbowEffect;
class WearableEffect;
class TclRenderer;
class TclManager;

// Represents time that can be used for scheduling purposes.
struct AdjustableTime {
  AdjustableTime();

  void AddMillis(int ms);

 private:
  uint64_t time_;
  friend class TclRenderer;
};

enum EffectMode {
  EFFECT_OVERLAY = 0,
  EFFECT_DUPLICATE = 1,
  EFFECT_MIRROR = 2,
};

// Sends requested images to TCL controller.
// To use this class:
//   renderer = new TclRenderer(controller_id, width, height, gamma);
//   renderer->SetLayout(layout);
//   // 'image_colors' has tuples with 4 RGBA bytes for each pixel,
//   //  with 'height' number of sequential rows, each row having
//   // length of 'width' pixels.
//   renderer->ScheduleImage(image_data, time);
class TclRenderer {
 public:
  // Configures a controller. Width and height define the size of
  // the intermediate image used internally. LED coordinates in
  // layout should be normalized to this image size.
  void AddController(
      int id, int width, int height,
      const LedLayout& layout, double gamma);
  void LockControllers();

  static TclRenderer* GetInstance() { return instance_; }

  void StartMessageLoop(int fps, bool enable_net);

  void SetGamma(double gamma);
  void SetGammaRanges(
      int r_min, int r_max, double r_gamma,
      int g_min, int g_max, double g_gamma,
      int b_min, int b_max, double b_gamma);

  void ResetImageQueue();

  void ScheduleImageAt(
      int controller_id, Bytes* bytes, int w, int h, EffectMode mode,
      int crop_x, int crop_y, int crop_w, int crop_h, int rotation_angle,
      int flip_mode, int id, const AdjustableTime& time, bool wakeup);
  void Wakeup();

  void SetEffectImage(
      int controller_id, Bytes* bytes, int w, int h, EffectMode mode);

  // Starts a rainbow at the give position. Pass -1 to disable.
  void EnableRainbow(int controller_id, int x);

  // Sets werable's effect number. Negative numbers set specific presets
  // from ProjectM.
  void SetWearableEffect(int id);

  Bytes* GetAndClearLastImage(int controller_id);
  Bytes* GetAndClearLastLedImage(int controller_id);
  int GetLastImageId(int controller_id);

  std::vector<int> GetFrameDataForTest(
      int controller_id, Bytes* bytes, int w, int h);

  // Returns the total of all artificial delays
  // added during sending of data.
  int GetFrameSendDuration();

  std::vector<int> GetAndClearFrameDelays();

  // Reset controller if no reply data in ms. Default is 5000.
  void SetAutoResetAfterNoDataMs(int value);

  void SetHdrMode(HdrMode mode);

  // Returns the number of currently queued frames.
  int GetQueueSize();

  std::string GetInitStatus();

 private:
  TclRenderer();
  TclRenderer(const TclRenderer& src);
  TclRenderer& operator=(const TclRenderer& rhs);
  ~TclRenderer();

  enum RenderingState {
    STATE_WEARABLE,
    STATE_VISUALIZATION,
  };

  struct ControllerInfo {
    ControllerInfo() : ControllerInfo(-1, -1) {}
    ControllerInfo(int width, int height);

    int width;
    int height;
    PassthroughEffect* passthrough_effect;
    FishifyEffect* fishify_effect;
    RainbowEffect* rainbow_effect;
    WearableEffect* wearable_effect;
    Effect* generic_effect;
  };

  typedef std::map<int, ControllerInfo> ControllerInfoMap;

  std::unique_ptr<RgbaImage> BuildImageLocked(
      const RgbaImage& input_img, EffectMode mode,
      int rotation_angle, int dst_w, int dst_h);
  void SetGenericEffect(int controller_id, Effect* effect, int priority);
  void UpdateWearableEffects();

  TclManager* tcl_manager_;
  ControllerInfoMap controllers_;
  int requested_wearable_effect_id_ = -1;
  int selected_wearable_effect_id_ = -2;
  RenderingState rendering_state_;
  uint64_t next_rendering_state_change_time_;
  uint64_t next_wearable_change_time_ = -1;

  static TclRenderer* instance_;
};

#endif  // __DFPLAYER_TCL_RENDERER_H
