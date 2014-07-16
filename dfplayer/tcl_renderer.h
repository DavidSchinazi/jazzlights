// Copyright 2014, Igor Chernyshev.
// Licensed under Apache 2.0 ASF license.
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
  friend class TclRenderer;
};

// Sends requested images to TCL controller.
// To use this class:
//   renderer = new TclRenderer(controller_id, width, height, gamma);
//   renderer->SetLayout(layout);
//   // 'image_colors' has tuples with 3 RGB bytes for each pixel,
//   //  with 'height' number of sequential rows, each row having
//   // length of 'width' pixels.
//   renderer->ScheduleImage(image_data, time);
class TclRenderer {
 public:
  TclRenderer(
      int controller_id, int width, int height,
      const Layout& layout, double gamma);
  ~TclRenderer();

  // TODO(igorc): Refactor TclRenderer collection.
  static TclRenderer* GetByControllerId(int id);

  void StartMessageLoop();

  void SetGamma(double gamma);
  void SetGammaRanges(
      int r_min, int r_max, double r_gamma,
      int g_min, int g_max, double g_gamma,
      int b_min, int b_max, double b_gamma);

  void ScheduleImageAt(Bytes* bytes, const AdjustableTime& time);

  std::vector<int> GetFrameDataForTest(Bytes* bytes);

  // Returns the total of all artificial delays
  // added during sending of data.
  int GetFrameSendDuration();

  std::vector<int> GetAndClearFrameDelays();

  // Reset controller if no reply data in ms. Default is 5000.
  void SetAutoResetAfterNoDataMs(int value);

  // Returns the number of currently queued frames.
  int GetQueueSize();

 private:
  TclRenderer(const TclRenderer& src);
  TclRenderer& operator=(const TclRenderer& rhs);

  struct Strands {
    Strands() {
      for (int i = 0; i < STRAND_COUNT; i++) {
        colors[i] = new uint8_t[STRAND_LENGTH * 3];
        lengths[i] = 0;
      }
    }

    ~Strands() {
      for (int i = 0; i < STRAND_COUNT; i++) {
        delete[] colors[i];
      }
    }

    uint8_t* colors[STRAND_COUNT];
    int lengths[STRAND_COUNT];

  private:
    Strands(const Strands& src);
    Strands& operator=(const Strands& rhs);
  };

  struct WorkItem {
    WorkItem(bool needs_reset, uint8_t* frame_data, uint64_t time)
        : needs_reset_(needs_reset), frame_data_(frame_data), time_(time) {}

    bool operator<(const WorkItem& other) const {
      return (time_ > other.time_);
    }

    bool needs_reset_;
    uint8_t* frame_data_;
    uint64_t time_;
  };

  void Run();
  static void* ThreadEntry(void* arg);

  // Socket communication funtions are invoked from worker thread only.
  bool Connect();
  void CloseSocket();
  bool InitController();
  bool SendFrame(uint8_t* frame_data);
  bool SendPacket(const void* data, int size);
  void ConsumeReplyData();
  void SetLastReplyTime();

  bool PopNextWorkItemLocked(WorkItem* item, int64_t* next_time);
  void WaitForQueueLocked(int64_t next_time);

  Strands* ConvertImageToStrands(uint8_t* image_data, int len);
  void ScheduleStrandsAt(Strands* strands, uint64_t time);
  void ApplyGamma(Strands* strands);

  static uint8_t* ConvertStrandsToFrame(Strands* strands);
  static int BuildFrameColorSeq(
    Strands* strands, int led_id, int color_component, uint8_t* dst);

  int controller_id_;
  int width_;
  int height_;
  int mgs_start_delay_us_;
  int mgs_data_delay_us_;
  int auto_reset_after_no_data_ms_;
  bool init_sent_;
  bool is_shutting_down_;
  bool has_started_thread_;
  double gamma_r_[256];
  double gamma_g_[256];
  double gamma_b_[256];
  Layout layout_;
  int socket_;
  bool require_reset_;
  uint64_t last_reply_time_;
  std::priority_queue<WorkItem> queue_;
  pthread_mutex_t lock_;
  pthread_cond_t cond_;
  pthread_t thread_;
  int frames_sent_after_reply_;
  std::vector<int> frame_delays_;

  static std::vector<TclRenderer*> all_renderers_;
};

#endif  // __DFPLAYER_TCL_RENDERER_H

