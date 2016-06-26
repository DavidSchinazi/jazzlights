// Copyright 2016, Igor Chernyshev.

#include "effects/wearable.h"

#include "util/lock.h"
#include "util/logging.h"

WearableEffect::WearableEffect() {}

WearableEffect::~WearableEffect() {}

void WearableEffect::DoInitialize() {}

void WearableEffect::Destroy() {
  CHECK(false);
}

void WearableEffect::SetEffect(int id) {
  (void) id;

  Autolock l(lock_);
}

void WearableEffect::ApplyOnImage(RgbaImage* dst, bool* is_done) {
  (void) dst;
  (void) is_done;

  Autolock l(lock_);
}
