// Copyright 2016, Igor Chernyshev.

#ifndef MODEL_PROJECTM_SOURCE_H_
#define MODEL_PROJECTM_SOURCE_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include <GL/glx.h>
#include <pthread.h>

#include "model/image_source.h"
#include "util/input_alsa.h"
#include "util/pixels.h"

class projectM;

class ProjectmSource : public ImageSource {
 public:
  ProjectmSource(int width, int height, int tex_size, int fps,
      const std::string& preset_dir, const std::string& textures_dir,
      int preset_duration);
  ~ProjectmSource() override;

  void StartMessageLoop();

  void UseAlsa(const std::string& spec);

  std::string GetCurrentPresetName();
  std::string GetCurrentPresetNameProgress();

  void SelectNextPreset();
  void SelectPreviousPreset();

  std::vector<std::string> GetPresetNames();

  void SetVolumeMultiplier(double value);
  double GetLastVolumeRms();

  // Returns [bass, bass_att, mid, mid_att, treb, treb_att].
  std::vector<double> GetLastBassInfo();

  int GetAndClearOverrunCount();
  std::vector<int> GetAndClearFramePeriods();
  bool GetAndClearHasNewImage();

  int tex_size() const { return tex_size_; }

  std::unique_ptr<RgbaImage> GetImage(int frame_id) override;

 private:
  ProjectmSource(const ProjectmSource& src);
  ProjectmSource& operator=(const ProjectmSource& rhs);

  class WorkItem {
   public:
    WorkItem() {}
    virtual ~WorkItem() = default;
    virtual void Run(ProjectmSource* self) = 0;
  };

  class NextPresetWorkItem : public WorkItem {
   public:
    NextPresetWorkItem(bool is_next) : is_next_(is_next) {}
    void Run(ProjectmSource* self) override;
   private:
    const bool is_next_;
  };

  static void* ThreadEntry(void* arg);

  void Run();
  void CreateRenderContext();
  void DestroyRenderContext();
  void CreateProjectM();
  bool RenderFrame(bool need_image);
  void CloseInputLocked();
  bool TransferPcmDataLocked();

  void ScheduleWorkItemLocked(WorkItem* item);

  pthread_mutex_t lock_;
  pthread_t thread_;
  bool is_shutting_down_ = false;
  bool has_started_thread_ = false;
  uint64_t last_render_time_;
  uint32_t ms_per_frame_;
  std::deque<WorkItem*> work_items_;
  std::vector<int> frame_periods_;
  bool has_new_image_ = false;

  std::string alsa_device_;
  AlsaInputHandle* alsa_handle_ = nullptr;
  int total_overrun_count_ = 0;
  double volume_multiplier_ = 1;
  double last_volume_rms_ = 0;

  int tex_size_;
  uint8_t* image_buffer_;
  uint32_t image_buffer_size_;

  projectM* projectm_ = nullptr;
  int projectm_tex_ = 0;
  std::string preset_dir_;
  std::string textures_dir_;
  int preset_duration_;
  std::string current_preset_;
  unsigned int current_preset_index_ = -1;
  std::vector<std::string> all_presets_;
  std::vector<double> last_bass_info_;

  // GLX data, used by the worker thread only.
  Display* display_;
  GLXContext gl_context_;
  GLXPbuffer pbuffer_;

  friend class NextPresetWorkItem;
};

#endif  // MODEL_PROJECTM_SOURCE_H_
