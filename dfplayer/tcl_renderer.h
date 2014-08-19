// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//
// TCL controller access module.

#ifndef __DFPLAYER_TCL_RENDERER_H
#define __DFPLAYER_TCL_RENDERER_H

#include <pthread.h>
#include <stdint.h>

#include <queue>
#include <vector>

#include "utils.h"

#define STRAND_COUNT    8
#define STRAND_LENGTH   512

class TclRenderer;
class TclController;

// Represents time that can be used for scheduling purposes.
struct AdjustableTime {
  AdjustableTime();

  void AddMillis(int ms);

 private:
  uint64_t time_;
  friend class TclRenderer;
};

// Contains coordinates of the LED's on strands.
struct Layout {
  Layout();

  void AddCoord(int strand_id, int x, int y);

 private:
  int x_[STRAND_COUNT][STRAND_LENGTH];
  int y_[STRAND_COUNT][STRAND_LENGTH];
  int lengths_[STRAND_COUNT];
  friend class TclController;
};

enum EffectMode {
  EFFECT_OVERLAY = 0,
  EFFECT_DUPLICATE = 1,
  EFFECT_MIRROR = 2,
};

enum HdrMode {
  HDR_NONE = 0,
  HDR_LUMINANCE = 1,
  HDR_SATURATION = 2,
  HDR_LSAT = 3,
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
      const Layout& layout, double gamma);
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

 private:
  TclRenderer();
  TclRenderer(const TclRenderer& src);
  TclRenderer& operator=(const TclRenderer& rhs);
  ~TclRenderer();

  struct WorkItem {
    WorkItem(bool needs_reset, TclController* controller,
             const RgbaImage& img, int id, uint64_t time)
        : needs_reset_(needs_reset), controller_(controller),
          img_(img), id_(id), time_(time) {}

    bool operator<(const WorkItem& other) const {
      return (time_ > other.time_);
    }

    bool needs_reset_;
    TclController* controller_;
    RgbaImage img_;
    int id_;
    uint64_t time_;
  };

  void Run();
  static void* ThreadEntry(void* arg);

  TclController* FindControllerLocked(int id);

  bool PopNextWorkItemLocked(WorkItem* item, int64_t* next_time);
  void WaitForQueueLocked(int64_t next_time);
  void WakeupLocked();

  std::vector<TclController*> controllers_;
  int fps_;
  int auto_reset_after_no_data_ms_;
  bool is_shutting_down_;
  bool has_started_thread_;
  bool enable_net_;
  bool controllers_locked_;
  uint64_t base_time_;
  std::priority_queue<WorkItem> queue_;
  pthread_mutex_t lock_;
  pthread_cond_t cond_;
  pthread_t thread_;
  std::vector<int> frame_delays_;

  static TclRenderer* instance_;
};

#endif  // __DFPLAYER_TCL_RENDERER_H

