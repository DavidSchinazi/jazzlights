// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//
// Visualization module.

#ifndef __DFPLAYER_VISUALIZER_H
#define __DFPLAYER_VISUALIZER_H

#include <pthread.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "utils.h"
#include "util/pixels.h"

class ProjectmSource;

class Visualizer {
 public:
  Visualizer(
      int width, int height, int tex_size, int fps,
      const std::string& preset_dir, const std::string& textures_dir,
      int preset_duration);
  ~Visualizer();

  void StartMessageLoop();

  void UseAlsa(const std::string& spec);
  void AddTargetController(
      int id, int effect_mode, int rotation_angle, int flip_mode);

  std::string GetCurrentPresetName();
  std::string GetCurrentPresetNameProgress();

  void SelectNextPreset();
  void SelectPreviousPreset();

  std::vector<std::string> GetPresetNames();

  int GetWidth() const { return width_; }
  int GetHeight() const { return height_; }
  int GetTexSize() const { return tex_size_; }

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

  struct ControllerInfo {
    ControllerInfo(int id, int effect_mode, int rotation_angle, int flip_mode)
        : id_(id), effect_mode_(effect_mode),
          rotation_angle_(rotation_angle), flip_mode_(flip_mode) {}

    int id_;
    int effect_mode_;
    int rotation_angle_;
    int flip_mode_;
  };

  void Run();
  static void* ThreadEntry(void* arg);

  void PostTclFrameLocked();

  int width_;
  int height_;
  int tex_size_;
  ProjectmSource* projectm_source_;
  std::vector<ControllerInfo> target_controllers_;
  uint64_t last_render_time_;
  uint32_t ms_per_frame_;
  bool is_shutting_down_ = false;
  bool has_started_thread_ = false;
  pthread_mutex_t lock_;
  pthread_t thread_;
};

#endif  // __DFPLAYER_VISUALIZER_H

