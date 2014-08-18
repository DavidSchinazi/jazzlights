// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//
// Visualization module.

#ifndef __DFPLAYER_VISUALIZER_H
#define __DFPLAYER_VISUALIZER_H

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <GL/glx.h>
#include <pthread.h>
#include <stdint.h>

#include <deque>
#include <vector>

#include "utils.h"
#include "input_alsa.h"
#include "../projectm/src/libprojectM/projectM.hpp"

class Visualizer {
 public:
  Visualizer(
      int width, int height, int texsize, int fps,
      const std::string& preset_dir, const std::string& textures_dir,
      int preset_duration);
  ~Visualizer();

  void StartMessageLoop();

  void UseAlsa(const std::string& spec);
  void AddTargetController(int id, int effect_mode, int rotation_angle);

  std::string GetCurrentPresetName();
  std::string GetCurrentPresetNameProgress();

  void SelectNextPreset();
  void SelectPreviousPreset();

  std::vector<std::string> GetPresetNames();

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  int GetTexSize() const { return texsize_; }

  Bytes* GetAndClearLastImageForTest();

  void SetVolumeMultiplier(double value);
  double GetLastVolumeRms();

  // Returns [bass, bass_att, mid, mid_att, treb, treb_att].
  std::vector<double> GetLastBassInfo();

  int GetAndClearOverrunCount();
  std::vector<int> GetAndClearFramePeriods();

 private:
  Visualizer(const Visualizer& src);
  Visualizer& operator=(const Visualizer& rhs);

  class WorkItem {
   public:
    WorkItem() {}
    virtual ~WorkItem() {}

    virtual void Run(Visualizer* self) = 0;
  };

  class NextPresetWorkItem : public WorkItem {
   public:
    NextPresetWorkItem(bool is_next) : is_next_(is_next) {}
    virtual void Run(Visualizer* self);
   private:
    bool is_next_;
  };

  struct ControllerInfo {
    ControllerInfo(int id, int effect_mode, int rotation_angle)
        : id_(id), effect_mode_(effect_mode),
          rotation_angle_(rotation_angle) {}

    int id_;
    int effect_mode_;
    int rotation_angle_;
  };

  void Run();
  static void* ThreadEntry(void* arg);

  void CloseInputLocked();
  bool TransferPcmDataLocked();
  bool RenderFrame(bool need_image);
  void PostTclFrameLocked();

  void CreateRenderContext();
  void DestroyRenderContext();
  void CreateProjectM();

  void ScheduleWorkItemLocked(WorkItem* item);

  int width_;
  int height_;
  int texsize_;
  int fps_;
  std::string alsa_device_;
  AlsaInputHandle* alsa_handle_;
  projectM* projectm_;
  int projectm_tex_;
  std::vector<ControllerInfo> target_controllers_;
  int total_overrun_count_;
  uint8_t* image_buffer_;
  uint32_t image_buffer_size_;
  bool has_image_;
  uint64_t last_render_time_;
  double volume_multiplier_;
  double last_volume_rms_;
  uint32_t ms_per_frame_;
  bool is_shutting_down_;
  bool has_started_thread_;
  std::deque<WorkItem*> work_items_;
  std::vector<int> frame_periods_;
  std::string preset_dir_;
  std::string textures_dir_;
  int preset_duration_;
  std::string current_preset_;
  unsigned int current_preset_index_;
  std::vector<std::string> all_presets_;
  std::vector<double> last_bass_info_;
  pthread_mutex_t lock_;
  pthread_t thread_;

  // GLX data, used by the worker thread only.
  Display* display_;
  GLXContext gl_context_;
  GLXPbuffer pbuffer_;

  friend class NextPresetWorkItem;
};

#endif  // __DFPLAYER_VISUALIZER_H

