// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
//
// TCL controller access module.

#include "tcl_renderer.h"

#include <math.h>
#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sstream>

#include "effects/fishify.h"
#include "effects/passthrough.h"
#include "effects/rainbow.h"
#include "effects/wearable.h"
#include "tcl/tcl_manager.h"
#include "util/logging.h"
#include "util/time.h"

namespace {

// We first run Fishify to update visualization image.
// The we run passthrough to overlay Python-provided images.
// At the end we run rainbow effects.
const int kFishifyEffectPriority = 1;
const int kWearableEffectPriority = 30;
const int kPassthroughEffectPriority = 20;
const int kRainbowEffectPriority = 10;

const uint64_t kWearableStateDurationMs = 300 * 1000;
const uint64_t kVisualizationStateDurationMs = 300 * 1000;
const uint64_t kWearableChangeDurationMs = 20 * 1000;

const int kWearableEffectIdsCount = 8;
const int kWearableEffectIds[kWearableEffectIdsCount] =
    {0, 1, 2, 3, 4, 5, 6, 7};

}  // namespace

TclRenderer* TclRenderer::instance_ = new TclRenderer();

TclRenderer::ControllerInfo::ControllerInfo(int width, int height)
    : width(width), height(height), rainbow_effect(nullptr),
      generic_effect(nullptr) {
  passthrough_effect = new PassthroughEffect();
  fishify_effect = new FishifyEffect();
  wearable_effect = new WearableEffect();
}

TclRenderer::TclRenderer() {
  tcl_manager_ = new TclManager();

  rendering_state_ = STATE_VISUALIZATION;
  next_rendering_state_change_time_ =
      GetCurrentMillis() + kVisualizationStateDurationMs;
}

TclRenderer::~TclRenderer() {
  delete tcl_manager_;
}

void TclRenderer::AddController(
    int id, int width, int height,
    const LedLayout& layout, double gamma) {
  tcl_manager_->AddController(id, width, height, layout, gamma);
  controllers_[id] = ControllerInfo(width, height);
  tcl_manager_->StartEffect(
      id, controllers_[id].passthrough_effect, kPassthroughEffectPriority);
  tcl_manager_->StartEffect(
      id, controllers_[id].fishify_effect, kFishifyEffectPriority);
  tcl_manager_->StartEffect(
      id, controllers_[id].wearable_effect, kWearableEffectPriority);
}

void TclRenderer::LockControllers() {
  tcl_manager_->LockControllers();
}

void TclRenderer::StartMessageLoop(int fps, bool enable_net) {
  tcl_manager_->StartMessageLoop(fps, enable_net);
}

void TclRenderer::SetGamma(double gamma) {
  // 1.0 is uncorrected gamma, which is perceived as "too bright"
  // in the middle. 2.4 is a good starting point. Changing this value
  // affects mid-range pixels - higher values produce dimmer pixels.
  SetGammaRanges(0, 255, gamma, 0, 255, gamma, 0, 255, gamma);
}

void TclRenderer::SetGammaRanges(
    int r_min, int r_max, double r_gamma,
    int g_min, int g_max, double g_gamma,
    int b_min, int b_max, double b_gamma) {
  tcl_manager_->SetGammaRanges(r_min, r_max, r_gamma,
			       g_min, g_max, g_gamma,
			       b_min, b_max, b_gamma);
}

void TclRenderer::SetHdrMode(HdrMode mode) {
  tcl_manager_->SetHdrMode(mode);
}

void TclRenderer::SetAutoResetAfterNoDataMs(int value) {
  tcl_manager_->SetAutoResetAfterNoDataMs(value);
}

std::string TclRenderer::GetInitStatus() {
  return tcl_manager_->GetInitStatus();
}

std::vector<int> TclRenderer::GetAndClearFrameDelays() {
  return tcl_manager_->GetAndClearFrameDelays();
}

int TclRenderer::GetFrameSendDuration() {
  return TclManager::GetFrameSendDurationMs();
}

Bytes* TclRenderer::GetAndClearLastImage(int controller_id) {
  std::unique_ptr<RgbaImage> img(
      tcl_manager_->GetAndClearLastImage(controller_id));
  return img ? new Bytes(img->data(), img->data_size()) : nullptr;
}

Bytes* TclRenderer::GetAndClearLastLedImage(int controller_id) {
  std::unique_ptr<RgbaImage> img(
      tcl_manager_->GetAndClearLastLedImage(controller_id));
  return img ? new Bytes(img->data(), img->data_size()) : nullptr;
}

int TclRenderer::GetLastImageId(int controller_id) {
  return tcl_manager_->GetLastImageId(controller_id);
}

void TclRenderer::ScheduleImageAt(
    int controller_id, Bytes* bytes, int w, int h, EffectMode mode,
    int crop_x, int crop_y, int crop_w, int crop_h, int rotation_angle,
    int flip_mode, int id, const AdjustableTime& time, bool wakeup) {
  if (bytes->GetLen() != RGBA_LEN(w, h)) {
    fprintf(stderr, "Unexpected image size in TCL renderer: %d\n",
            bytes->GetLen());
    return;
  }

  int controller_w = -1;
  int controller_h = -1;
  if (!tcl_manager_->GetControllerImageSize(
          controller_id, &controller_w, &controller_h)) {
    fprintf(stderr, "ScheduleImageAt controller not found: %d\n", controller_id);
    return;
  }

  // Updating wearable effect id could be done on some more relaxed schedule,
  // but given internal optimizations in that method, it's OK to call often.
  UpdateWearableEffects();

  uint8_t* cropped_img = bytes->GetData();
  if (flip_mode == 1) {
    cropped_img = FlipImage(cropped_img, w, h, true);
  } else if (flip_mode == 2) {
    cropped_img = FlipImage(cropped_img, w, h, false);
  }

  if (crop_x != 0 || crop_y != 0 || crop_w != w || crop_h != h) {
    uint8_t* old_img = cropped_img;
    cropped_img = CropImage(
        cropped_img, w, h, crop_x, crop_y, crop_w, crop_h);
    if (old_img != bytes->GetData())
      delete[] old_img;
  } else {
    crop_w = w;
    crop_h = h;
  }

  RgbaImage input_img(cropped_img, crop_w, crop_h);
  std::unique_ptr<RgbaImage> render_img(BuildImageLocked(
      input_img, mode, rotation_angle, controller_w, controller_h));

  tcl_manager_->ScheduleImageAt(
      controller_id, *render_img.get(), id, time.time_, wakeup);

  if (cropped_img != bytes->GetData())
    delete[] cropped_img;
}

std::unique_ptr<RgbaImage> TclRenderer::BuildImageLocked(
    const RgbaImage& input_img, EffectMode mode,
    int rotation_angle, int dst_w, int dst_h) {
  if (input_img.empty())
    return std::unique_ptr<RgbaImage>();

  const uint8_t* src_img = input_img.data();
  int src_w = input_img.width();
  int src_h = input_img.height();
  if (rotation_angle != 0)
    src_img = RotateImage(src_img, src_w, src_h, src_h, src_w, rotation_angle);

  // We expect all incoming images to use linearized RGB gamma.
  std::unique_ptr<RgbaImage> dst(new RgbaImage());
  dst->ResizeStorage(dst_w, dst_h);
  if (mode == EFFECT_OVERLAY) {
    ResizeImage(src_img, src_w, src_h, dst->data(), dst_w, dst_h);
  } else if (mode == EFFECT_DUPLICATE) {
    uint8_t* img1 = ResizeImage(src_img, src_w, src_h, dst_w / 2, dst_h);
    PasteSubImage(
        img1, dst_w / 2, dst_h,
        dst->data(), 0, 0, dst_w, dst_h, false, true);
    PasteSubImage(
        img1, dst_w / 2, dst_h,
        dst->data(), dst_w / 2, 0, dst_w, dst_h, false, true);
    delete[] img1;
  } else {  // EFFECT_MIRROR
    uint8_t* img1 = ResizeImage(
        src_img, src_w, src_h, dst_w / 2, dst_h);
    uint8_t* img2 = FlipImage(img1, dst_w / 2, dst_h, true);
    PasteSubImage(
        img1, dst_w / 2, dst_h,
        dst->data(), 0, 0, dst_w, dst_h, false, true);
    PasteSubImage(
        img2, dst_w / 2, dst_h,
        dst->data(), dst_w / 2, 0, dst_w, dst_h, false, true);
    delete[] img1;
    delete[] img2;
  }

  if (src_img != input_img.data())
    delete[] src_img;

  return dst;
}

void TclRenderer::Wakeup() {
  tcl_manager_->Wakeup();
}

void TclRenderer::SetEffectImage(
    int controller_id, Bytes* bytes, int w, int h, EffectMode mode) {
  if (bytes->GetLen() != RGBA_LEN(w, h)) {
    fprintf(stderr, "Unexpected image size in TCL renderer effect: %d\n",
            bytes->GetLen());
    return;
  }

  if (!controllers_.count(controller_id)) {
    fprintf(stderr, "SetEffectImage controller not found: %d\n", controller_id);
    return;
  }

  const ControllerInfo& controller = controllers_[controller_id];

  RgbaImage input_img(bytes->GetData(), w, h);
  if (input_img.empty()) {
    controller.passthrough_effect->SetImage(input_img);
    return;
  }

  int rotation_angle = 0;
  std::unique_ptr<RgbaImage> render_img(BuildImageLocked(
      input_img, mode, rotation_angle, controller.width, controller.height));
  controller.passthrough_effect->SetImage(*render_img.get());
}

void TclRenderer::SetWearableEffect(int id) {
  requested_wearable_effect_id_ = id;
}

void TclRenderer::UpdateWearableEffects() {
  uint64_t now = GetCurrentMillis();
  if (now >= next_rendering_state_change_time_) {
    if (rendering_state_ == STATE_VISUALIZATION) {
      rendering_state_ = STATE_WEARABLE;
      next_rendering_state_change_time_ = now + kWearableStateDurationMs;
      fprintf(stderr, "Switched to STATE_WEARABLE\n");
    } else {
      rendering_state_ = STATE_VISUALIZATION;
      next_rendering_state_change_time_ = now + kVisualizationStateDurationMs;
      fprintf(stderr, "Switched to STATE_VISUALIZATION\n");
    }
    next_wearable_change_time_ = now;
  }

  int prev_wearable_effect_id = selected_wearable_effect_id_;
  if (requested_wearable_effect_id_ >= 0) {
    selected_wearable_effect_id_ = requested_wearable_effect_id_;
  } else if (rendering_state_ == STATE_VISUALIZATION) {
    selected_wearable_effect_id_ = -1;
  } else if (now >= next_wearable_change_time_) {
    int idx = rand() % kWearableEffectIdsCount;
    selected_wearable_effect_id_ = kWearableEffectIds[idx];
    next_wearable_change_time_ = now + kWearableChangeDurationMs;
  }

  if (selected_wearable_effect_id_ == prev_wearable_effect_id)
    return;

  fprintf(stderr, "Setting wearable effect %d\n", selected_wearable_effect_id_);
  for (auto& it : controllers_) {
    it.second.wearable_effect->SetEffect(selected_wearable_effect_id_);
  }
}

void TclRenderer::EnableRainbow(int controller_id, int x) {
  if (!controllers_.count(controller_id))
    return;

  ControllerInfo& controller = controllers_[controller_id];

  if (x < 0) {
    if (controller.rainbow_effect) {
      controller.rainbow_effect->Stop();
      controller.rainbow_effect = nullptr;
    }
    return;
  }

  if (controller.rainbow_effect) {
    controller.rainbow_effect->SetX(x);
    return;
  }

  controller.rainbow_effect = new RainbowEffect(x);
  tcl_manager_->StartEffect(
      controller_id, controller.rainbow_effect, kRainbowEffectPriority);
}

void TclRenderer::SetGenericEffect(
    int controller_id, Effect* effect, int priority) {
  if (!controllers_.count(controller_id))
    return;

  ControllerInfo& controller = controllers_[controller_id];

  if (controller.generic_effect == effect)
    return;

  if (controller.generic_effect)
    controller.generic_effect->Stop();

  controller.generic_effect = effect;

  if (controller.generic_effect) {
    EnableRainbow(controller_id, -1);
    tcl_manager_->StartEffect(
	controller_id, controller.generic_effect, priority);
  }
}

void TclRenderer::ResetImageQueue() {
  tcl_manager_->ResetImageQueue();
}

std::vector<int> TclRenderer::GetFrameDataForTest(
    int controller_id, Bytes* bytes, int w, int h) {
  RgbaImage image(bytes->GetData(), w, h);
  return tcl_manager_->GetFrameDataForTest(controller_id, image);
}

int TclRenderer::GetQueueSize() {
  return tcl_manager_->GetQueueSize();
}

///////////////////////////////////////////////////////////////////////////////
// Misc classes
///////////////////////////////////////////////////////////////////////////////

AdjustableTime::AdjustableTime() : time_(GetCurrentMillis()) {}

void AdjustableTime::AddMillis(int ms) {
  time_ += ms;
}
