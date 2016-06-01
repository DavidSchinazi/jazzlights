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

#include "tcl/tcl_manager.h"
#include "util/logging.h"
#include "util/time.h"

TclRenderer* TclRenderer::instance_ = new TclRenderer();

TclRenderer::TclRenderer() {
  tcl_manager_ = new TclManager();
}

TclRenderer::~TclRenderer() {
  delete tcl_manager_;
}

void TclRenderer::AddController(
    int id, int width, int height,
    const LedLayout& layout, double gamma) {
  tcl_manager_->AddController(id, width, height, layout, gamma);
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
  return new Bytes(img->data(), img->data_size());
}

Bytes* TclRenderer::GetAndClearLastLedImage(int controller_id) {
  std::unique_ptr<RgbaImage> img(
      tcl_manager_->GetAndClearLastLedImage(controller_id));
  return new Bytes(img->data(), img->data_size());
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

  int controller_w = -1;
  int controller_h = -1;
  if (!tcl_manager_->GetControllerImageSize(
          controller_id, &controller_w, &controller_h)) {
    fprintf(stderr, "SetEffectImage controller not found: %d\n", controller_id);
    return;
  }

  RgbaImage input_img(bytes->GetData(), w, h);
  if (input_img.empty()) {
    tcl_manager_->SetEffectImage(controller_id, input_img);
    return;
  }

  int rotation_angle = 0;
  std::unique_ptr<RgbaImage> render_img(BuildImageLocked(
      input_img, mode, rotation_angle, controller_w, controller_h));
  tcl_manager_->SetEffectImage(controller_id, *render_img.get());
}

void TclRenderer::EnableRainbow(int controller_id, int x) {
  tcl_manager_->EnableRainbow(controller_id, x);
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
