// Copyright 2016, Igor Chernyshev.

#ifndef MODEL_EFFECT_H_
#define MODEL_EFFECT_H_

#include <pthread.h>

#include "util/pixels.h"

class LedStrands;

// Defines base abstraction for effects. One instance can only be used
// with one rendering surface such as TclController. Surface's dimensions
// and FPS for calling Apply() are passed to Initialize() call.
// Apply() and Destroy() are accessed by surface rendering thread.
class Effect {
 public:
  Effect();
  virtual ~Effect();

  // Can be invoked by any thread to abort the effect.
  void Stop();
  bool IsStopped() const;

  // Invoked by the rendering surface.
  void Initialize(int width, int height, int fps);

  virtual void ApplyOnImage(RgbaImage* dst, bool* is_done) {
    (void) dst;
    (void) is_done;
  }

  virtual void ApplyOnLeds(LedStrands* strands, bool* is_done) {
    (void) strands;
    (void) is_done;
  }

  // Invoked by the surface when |is_done| becomes true.
  virtual void Destroy() = 0;

 protected:
  virtual void DoInitialize() {}

  // Helper function to merge images.
  void MergeImage(RgbaImage* dst, const RgbaImage& src);

  int width() const { return width_; }
  int height() const { return height_; }
  int fps() const { return fps_; }

  mutable pthread_mutex_t lock_;

 private:
  Effect(const Effect& src);
  Effect& operator=(const Effect& rhs);

  bool initialized_ = false;
  volatile bool stopped_ = false;
  int width_ = -1;
  int height_ = -1;
  int fps_ = -1;
};

#endif  // MODEL_EFFECT_H_
