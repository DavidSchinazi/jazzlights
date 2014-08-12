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
//   // 'image_colors' has tuples with 4 RGBA bytes for each pixel,
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

  void StartMessageLoop(bool enable_net);

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }

  void SetGamma(double gamma);
  void SetGammaRanges(
      int r_min, int r_max, double r_gamma,
      int g_min, int g_max, double g_gamma,
      int b_min, int b_max, double b_gamma);

  void ResetImageQueue();

  void ScheduleImageAt(Bytes* bytes, int id, const AdjustableTime& time);

  void SetEffectImage(Bytes* bytes, bool mirror);

  Bytes* GetAndClearLastImage();
  int GetLastImageId();

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
    WorkItem(bool needs_reset, uint8_t* img, int id, uint64_t time)
        : needs_reset_(needs_reset), img_(img), id_(id), time_(time) {}

    bool operator<(const WorkItem& other) const {
      return (time_ > other.time_);
    }

    bool needs_reset_;
    uint8_t* img_;
    int id_;
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

  uint8_t* CreateAdjustedImageLocked(Bytes* bytes, int w, int h);
  void ApplyEffectLocked(uint8_t* image);

  Strands* DiffuseAndConvertImageToStrandsLocked(uint8_t* image_data);

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
  bool enable_net_;
  RgbGamma gamma_;
  Layout layout_;
  int socket_;
  bool require_reset_;
  uint64_t last_reply_time_;
  uint8_t* last_image_;
  int last_image_id_;
  bool has_last_image_;
  uint8_t* effect_image_;
  bool effect_image_mirrored_;
  std::priority_queue<WorkItem> queue_;
  pthread_mutex_t lock_;
  pthread_cond_t cond_;
  pthread_t thread_;
  int frames_sent_after_reply_;
  std::vector<int> frame_delays_;

  static std::vector<TclRenderer*> all_renderers_;
};

#endif  // __DFPLAYER_TCL_RENDERER_H

