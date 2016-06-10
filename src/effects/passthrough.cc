// Copyright 2016, Igor Chernyshev.

#include "effects/passthrough.h"

#include "util/lock.h"
#include "util/logging.h"

PassthroughEffect::PassthroughEffect() {}

PassthroughEffect::~PassthroughEffect() {}

void PassthroughEffect::DoInitialize() {}

void PassthroughEffect::Destroy() {
  CHECK(false);
}

void PassthroughEffect::SetImage(const RgbaImage& image) {
  Autolock l(lock_);
  image_ = image;
}

void PassthroughEffect::Apply(RgbaImage* dst, bool* is_done) {
  (void) is_done;

  Autolock l(lock_);
  MergeImage(dst, image_);
}
