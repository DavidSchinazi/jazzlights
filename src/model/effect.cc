// Copyright 2016, Igor Chernyshev.

#include "model/effect.h"

#include "util/lock.h"
#include "util/logging.h"

Effect::Effect() : lock_(PTHREAD_MUTEX_INITIALIZER) {}

Effect::~Effect() {
  pthread_mutex_destroy(&lock_);
}

void Effect::Initialize(int width, int height, int fps) {
  CHECK(!initialized_);
  initialized_ = true;
  width_ = width;
  height_ = height;
  fps_ = fps;

  DoInitialize();
}

void Effect::Stop() {
  Autolock l(lock_);
  stopped_ = true;
}

bool Effect::IsStopped() const {
  Autolock l(lock_);
  return stopped_;
}

void Effect::MergeImage(RgbaImage* dst, const RgbaImage& src) {
  if (src.empty())
    return;

  PasteSubImage(
      src.data(), width_, height_, dst->data(), 0, 0, width_, height_,
      true, true);
}
