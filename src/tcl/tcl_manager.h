// Copyright 2014, Igor Chernyshev.

#ifndef TCL_TCL_MANAGER_H_
#define TCL_TCL_MANAGER_H_

#include <pthread.h>
#include <stdint.h>

#include <queue>

#include "tcl/tcl_types.h"
#include "util/led_layout.h"
#include "util/pixels.h"

class Effect;
class TclController;

class TclManager {
 public:
  TclManager();
  ~TclManager();

  void StartMessageLoop(int fps, bool enable_net);

  void Wakeup();

  // Configures a controller. Width and height define the size of
  // the intermediate image used internally. LED coordinates in
  // layout should be normalized to this image size.
  void AddController(
      int id, int width, int height,
      const LedLayout& layout, double gamma);
  void LockControllers();

  bool GetControllerImageSize(int controller_id, int* w, int* h);

  void SetGammaRanges(
      int r_min, int r_max, double r_gamma,
      int g_min, int g_max, double g_gamma,
      int b_min, int b_max, double b_gamma);

  void ScheduleImageAt(
      int controller_id, const RgbaImage& image, int id,
      uint64_t time, bool wakeup);

  void StartEffect(int controller_id, Effect* effect, int priority);

  std::unique_ptr<RgbaImage> GetAndClearLastImage(int controller_id);
  std::unique_ptr<RgbaImage> GetAndClearLastLedImage(int controller_id);
  int GetLastImageId(int controller_id);

  std::vector<int> GetFrameDataForTest(
      int controller_id, const RgbaImage& image);

  std::vector<int> GetAndClearFrameDelays();

  // Reset controller if no reply data in ms. Default is 5000.
  void SetAutoResetAfterNoDataMs(int value);

  void SetHdrMode(HdrMode mode);

  // Returns the number of currently queued frames.
  int GetQueueSize();

  std::string GetInitStatus();
  void ResetImageQueue();

  // Returns the total of all artificial delays
  // added during sending of data.
  static int GetFrameSendDurationMs();

 private:
  TclManager(const TclManager& src);
  TclManager& operator=(const TclManager& rhs);

  struct WorkItem {
    WorkItem(bool needs_reset, TclController* controller,
	     const RgbaImage& img, int id, uint64_t time)
        : needs_reset(needs_reset), controller(controller),
          img(img), id(id), time(time) {}

    bool operator<(const WorkItem& other) const {
      return (time > other.time);
    }

    bool needs_reset;
    TclController* controller;
    RgbaImage img;
    int id;
    uint64_t time;
  };

  void Run();
  static void* ThreadEntry(void* arg);

  TclController* FindControllerLocked(int id);

  bool PopNextWorkItemLocked(WorkItem* item, int64_t* next_time);
  void WaitForQueueLocked(int64_t next_time);
  void WakeupLocked();

  int fps_ = 15;
  int auto_reset_after_no_data_ms_ = 5000;
  bool is_shutting_down_ = false;
  bool has_started_thread_ = false;
  bool enable_net_ = false;
  bool controllers_locked_ = false;
  uint64_t base_time_;
  std::priority_queue<WorkItem> queue_;
  pthread_mutex_t lock_;
  pthread_cond_t cond_;
  pthread_t thread_;
  std::vector<int> frame_delays_;
  std::vector<TclController*> controllers_;
};

#endif  // TCL_TCL_MANAGER_H_
