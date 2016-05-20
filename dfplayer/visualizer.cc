// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//
// Visualization module.

#include "visualizer.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>

//#include <csignal>

#include "model/projectm_source.h"
#include "tcl_renderer.h"
#include "util/lock.h"
#include "util/logging.h"
#include "util/time.h"

Visualizer::Visualizer(
    int width, int height, int tex_size, int fps,
    const std::string& preset_dir, const std::string& textures_dir,
    int preset_duration)
    : width_(width), height_(height), lock_(PTHREAD_MUTEX_INITIALIZER) {
  last_render_time_ = GetCurrentMillis();
  ms_per_frame_ = (uint32_t) (1000.0 / fps);

  projectm_source_ = new ProjectmSource(
      width, height, tex_size, fps, preset_dir, textures_dir, preset_duration);
  tex_size_ = projectm_source_->tex_size();
}

Visualizer::~Visualizer() {
  if (has_started_thread_) {
    {
      Autolock l(lock_);
      is_shutting_down_ = true;
    }

    pthread_join(thread_, NULL);
  }

  delete projectm_source_;

  pthread_mutex_destroy(&lock_);
}

void Visualizer::StartMessageLoop() {
  projectm_source_->StartMessageLoop();

  Autolock l(lock_);
  if (has_started_thread_)
    return;
  has_started_thread_ = true;
  int err = pthread_create(&thread_, NULL, &ThreadEntry, this);
  if (err != 0) {
    fprintf(stderr, "pthread_create failed with %d\n", err);
    CHECK(false);
  }
}

void Visualizer::UseAlsa(const std::string& spec) {
  projectm_source_->UseAlsa(spec);
}

void Visualizer::AddTargetController(
    int id, int effect_mode, int rotation_angle, int flip_mode) {
  Autolock l(lock_);
  target_controllers_.push_back(
      ControllerInfo(id, effect_mode, rotation_angle, flip_mode));
}

std::string Visualizer::GetCurrentPresetName() {
  return projectm_source_->GetCurrentPresetName();
}

std::string Visualizer::GetCurrentPresetNameProgress() {
  return projectm_source_->GetCurrentPresetNameProgress();
}

std::vector<std::string> Visualizer::GetPresetNames() {
  return projectm_source_->GetPresetNames();
}

void Visualizer::SelectNextPreset() {
  projectm_source_->SelectNextPreset();
}

void Visualizer::SelectPreviousPreset() {
  projectm_source_->SelectPreviousPreset();
}

int Visualizer::GetAndClearOverrunCount() {
  return projectm_source_->GetAndClearOverrunCount();
}

std::vector<int> Visualizer::GetAndClearFramePeriods() {
  return projectm_source_->GetAndClearFramePeriods();
}

Bytes* Visualizer::GetAndClearLastImageForTest() {
  if (!projectm_source_->GetAndClearHasNewImage())
    return NULL;
  /*int dst_w = 500;
  int src_w = 250;
  int height = 50;
  uint8_t* src_img1 = ResizeImage(
      image_buffer_, tex_size_, tex_size_, src_w, height);
  uint8_t* src_img2 = FlipImage(src_img1, src_w, height, true);

  int len = PIX_LEN(dst_w, height);
  uint8_t* dst = new uint8_t[len];
  PasteSubImage(src_img1, src_w, height,
      dst, 0, 0, dst_w, height, false);
  PasteSubImage(src_img2, src_w, height,
      dst, src_w, 0, dst_w, height, false);

  Bytes* result = new Bytes(dst, len);
  delete[] src_img1;
  delete[] src_img2;
  delete[] dst;
  return result;*/
  std::unique_ptr<RgbaImage> image = projectm_source_->GetImage(-1);
  return new Bytes(image->GetData(), image->GetDataLen());
}

void Visualizer::SetVolumeMultiplier(double value) {
  projectm_source_->SetVolumeMultiplier(value);
}

double Visualizer::GetLastVolumeRms() {
  return projectm_source_->GetLastVolumeRms();
}

std::vector<double> Visualizer::GetLastBassInfo() {
  return projectm_source_->GetLastBassInfo();
}

void Visualizer::PostTclFrameLocked() {
  static const int kCropWidth = 4;

  TclRenderer* tcl = TclRenderer::GetInstance();
  if (!tcl) {
    fprintf(stderr, "TCL renderer not found\n");
    return;
  }

  AdjustableTime now;
  // TODO(igorc): Reduce data copies.
  std::unique_ptr<RgbaImage> image = projectm_source_->GetImage(-1);
  Bytes* bytes = new Bytes(image->GetData(), image->GetDataLen());
  for (size_t i = 0; i < target_controllers_.size(); ++i) {
    const ControllerInfo& controller = target_controllers_[i];
    tcl->ScheduleImageAt(
        controller.id_, bytes, tex_size_, tex_size_,
        (EffectMode) controller.effect_mode_,
        kCropWidth, kCropWidth,
	tex_size_ - kCropWidth * 2, tex_size_ - kCropWidth * 2,
        controller.rotation_angle_, controller.flip_mode_,
        0, now, false);
  }
  // Wakeup only once to avoid concurrent locking.
  tcl->Wakeup();
  delete bytes;
}

// static
void* Visualizer::ThreadEntry(void* arg) {
  Visualizer* self = reinterpret_cast<Visualizer*>(arg);
  self->Run();
  return NULL;
}

void Visualizer::Run() {
  uint64_t next_render_time = GetCurrentMillis() + ms_per_frame_;
  uint32_t frame_num = 0;
  while (true) {
    uint32_t remaining_time = 0;
    {
      Autolock l(lock_);
      if (is_shutting_down_)
        break;
      uint64_t now = GetCurrentMillis();
      if (next_render_time > now)
        remaining_time = next_render_time - now;
    }

    if (remaining_time > 0) {
      Sleep(((double) remaining_time) / 1000.0);
      continue;
    }

    {
      Autolock l(lock_);
      if (is_shutting_down_)
        break;

      PostTclFrameLocked();
      next_render_time += ms_per_frame_;
    }

    frame_num++;
  }
}
