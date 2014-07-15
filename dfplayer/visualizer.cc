// Copyright 2014, Igor Chernyshev.
// Licensed under Apache 2.0 ASF license.
//
// Visualization module.

#include "visualizer.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

//#include <csignal>

Visualizer::Visualizer(int width, int height, int fps)
    : width_(width), height_(height), fps_(fps), alsa_handle_(NULL),
      total_overrun_count_(0), has_image_(false), dropped_image_count_(0),
      is_shutting_down_(false), has_started_thread_(false),
      lock_(PTHREAD_MUTEX_INITIALIZER) {
  last_render_time_ = GetCurrentMillis();
  ms_per_frame_ = (uint32_t) (1000.0 / (double) fps);

  pcm_buffer_ = new int16_t[PCM::maxsamples * 2];

  image_buffer_size_ = width_ * height_ * 3;
  image_buffer_ = new uint8_t[image_buffer_size_];
}

Visualizer::~Visualizer() {
  if (has_started_thread_) {
    {
      Autolock l(lock_);
      is_shutting_down_ = true;
    }

    pthread_join(thread_, NULL);
  }

  CloseInputLocked();

  while (!work_items_.empty()) {
    WorkItem* item = work_items_.front();
    work_items_.pop_front();
    delete item;
  }

  pthread_mutex_destroy(&lock_);

  delete[] pcm_buffer_;
  delete[] image_buffer_;
}

void Visualizer::StartMessageLoop() {
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
  Autolock l(lock_);
  CloseInputLocked();
  alsa_device_ = spec;
}

std::vector<std::string> Visualizer::GetPresetNames() {
  return std::vector<std::string>();
}

void Visualizer::UsePreset(const std::string& spec) {
}

int Visualizer::GetAndClearOverrunCount() {
  Autolock l(lock_);
  int result = total_overrun_count_;
  total_overrun_count_ = 0;
  return result;
}

int Visualizer::GetAndClearTotalPcmSampleCount() {
  Autolock l(lock_);
  int result = total_pcm_sample_count_;
  total_pcm_sample_count_ = 0;
  return result;
}

int Visualizer::GetAndClearDroppedImageCount() {
  Autolock l(lock_);
  int result = dropped_image_count_;
  dropped_image_count_ = 0;
  return result;
}

std::vector<int> Visualizer::GetAndClearFramePeriods() {
  Autolock l(lock_);
  std::vector<int> result = frame_periods_;
  frame_periods_.clear();
  return result;
}

Bytes* Visualizer::GetAndClearImage() {
  Autolock l(lock_);
  if (!has_image_)
    return NULL;
  has_image_ = false;
  uint8_t* buf = new uint8_t[image_buffer_size_];
  memcpy(buf, image_buffer_, image_buffer_size_);
  return new Bytes(buf, image_buffer_size_);
}

void Visualizer::CloseInputLocked() {
  if (alsa_handle_) {
    inp_alsa_cleanup(alsa_handle_);
    alsa_handle_ = NULL;
  }
}

bool Visualizer::TransferPcmDataLocked() {
  if (!alsa_handle_) {
    if (alsa_device_.empty())
      return false;
    fprintf(stderr, "Connecting to ALSA input %s\n", alsa_device_.c_str());
    alsa_handle_ = inp_alsa_init(alsa_device_.c_str());
    if (!alsa_handle_)
      return false;
  }

  int sample_count = 0;
  PCM* pcm = projectm_->pcm();
  while (sample_count < PCM::maxsamples) {
    int overrun_count = 0;
    int samples = inp_alsa_read(
        alsa_handle_, pcm_buffer_ + sample_count * 2,
        PCM::maxsamples - sample_count, &overrun_count);
    total_overrun_count_ += overrun_count;
    if (samples <= 0)
      break;
    sample_count += samples;
  }

  //fprintf(stderr, "Adding %d samples\n", sample_count);
  total_pcm_sample_count_ += sample_count;
  pcm->addPCM16Data(pcm_buffer_, sample_count);

  return true;
}

void Visualizer::CreateRenderContext() {
  display_ = XOpenDisplay(NULL);
  if (!display_) {
    fprintf(stderr, "Unable to open display\n");
    CHECK(false);
  }

  static int kVisualAttribs[] = { None };
  int fb_config_count = 0;
  GLXFBConfig* fb_configs = glXChooseFBConfig(
      display_, DefaultScreen(display_), kVisualAttribs, &fb_config_count);
  if (!fb_configs) {
    fprintf(stderr, "Unable to find FB config\n");
    CHECK(false);
  }

  /*static int kContextAttribs[] = {
      GLX_CONTEXT_MAJOR_VERSION_ARB,  4,
      GLX_CONTEXT_MINOR_VERSION_ARB,  2,
      GLX_CONTEXT_FLAGS_ARB,          GLX_CONTEXT_DEBUG_BIT_ARB,
      GLX_CONTEXT_PROFILE_MASK_ARB,   GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
      None
  };
  gl_context_ = glXCreateContextAttribsARB(
      display_, fb_configs[0], 0, true, kContextAttribs);*/
  gl_context_ = glXCreateNewContext(
      display_, fb_configs[0], GLX_RGBA_TYPE, NULL, true);
  if (!gl_context_) {
    fprintf(stderr, "Unable to create GL context\n");
    CHECK(false);
  }

  int pbuffer_attribs[] = {
      GLX_PBUFFER_WIDTH,  width_,
      GLX_PBUFFER_HEIGHT, height_,
      None
  };
  pbuffer_ = glXCreatePbuffer(display_, fb_configs[0], pbuffer_attribs);
  if (!pbuffer_) {
    fprintf(stderr, "Unable to create Pbuffer\n");
    CHECK(false);
  }

  XFree(fb_configs);
  XSync(display_, false);

  if (!glXMakeContextCurrent(display_, pbuffer_, pbuffer_, gl_context_)) {
    fprintf(stderr, "Unable to set GL context and pbuffer as current\n");
    CHECK(false);
  }
}

void Visualizer::DestroyRenderContext() {
  glXMakeContextCurrent(display_, None, None, NULL);
  glXDestroyContext(display_, gl_context_);
  glXDestroyPbuffer(display_, pbuffer_);
  XCloseDisplay(display_);
}

void Visualizer::CreateProjectM() {
  //raise(SIGINT);

  // TODO(igorc): Tune the settings.
  // TODO(igorc): Consider disabling threads in CMakeCache.txt.
  //              Threads are used for evaluating the second preset.
  projectM::Settings settings;
  settings.windowWidth = width_;
  settings.windowHeight = height_;
  settings.fps = fps_;
  settings.textureSize = 256;
  settings.meshX = width_;
  settings.meshY = height_;
  settings.presetURL = "dfplayer/presets";
  settings.titleFontURL = "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf";
  settings.menuFontURL = "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf";
  settings.beatSensitivity = 10;
  settings.aspectCorrection = 1;
  // Preset duration is based on gaussian distribution
  // with mean of |presetDuration| and sigma of |easterEgg|.
  settings.presetDuration = 30;
  settings.easterEgg = 1;
  // Transition period for switching between presets.
  settings.smoothPresetDuration = 5;
  settings.shuffleEnabled = 1;
  settings.softCutRatingsEnabled = 0;
  projectm_ = new projectM(settings);
}

bool Visualizer::RenderFrameLocked() {
  projectm_->renderFrame();

  //glFlush();
  glXSwapBuffers(display_, pbuffer_);
  //glReadBuffer(GL_BACK);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  //glReadPixels(0, 0, width_, height_, GL_RGB, GL_UNSIGNED_BYTE, image_buffer_);
  // http://stackoverflow.com/questions/12157646/how-to-render-offscreen-on-opengl
  // Thanks to all, it works now, the good way was to render into a Pixmap with
  // eglCreatePixmapSurface method and then read easily its pixels.
  // Now it runs @ 45FPS and not @ 17FPS with glReadPixels... exactly as I wanted...
  GLenum err = glGetError();
  if (err == GL_NO_ERROR) {
    return true;
  }

  GLenum status = 0; //glCheckFramebufferStatus(GL_FRAMEBUFFER);
  fprintf(stderr, "Unable to read pixels, err=0x%x, fb_status=0x%x\n",
          err, status);
  return false;
}

static void Sleep(double seconds) {
  struct timespec req;
  struct timespec rem;
  req.tv_sec = (int) seconds;
  req.tv_nsec = (long) ((seconds - req.tv_sec) * 1000000000.0);
  rem.tv_sec = 0;
  rem.tv_nsec = 0;
  while (nanosleep(&req, &rem) == -1) {
    if (errno != EINTR) {
      REPORT_ERRNO("nanosleep");
      break;
    }
    req = rem;
  }
}

// static
void* Visualizer::ThreadEntry(void* arg) {
  Visualizer* self = reinterpret_cast<Visualizer*>(arg);
  self->Run();
  return NULL;
}

void Visualizer::Run() {
  CreateRenderContext();
  CreateProjectM();

  bool should_sleep = false;
  uint64_t next_render_time = GetCurrentMillis() + ms_per_frame_;
  uint64_t prev_frame_time = 0;
  while (true) {
    if (should_sleep) {
      should_sleep = false;
      Sleep(0.1);
    }

    uint32_t remaining_time = 0;
    {
      Autolock l(lock_);
      if (is_shutting_down_)
        break;
      uint64_t now = GetCurrentMillis();
      if (next_render_time > now)
        remaining_time = next_render_time - now;
    }

    if (remaining_time > 0)
      Sleep(((double) remaining_time) / 1000.0);

    {
      Autolock l(lock_);
      if (is_shutting_down_)
        break;

      while (!work_items_.empty()) {
        WorkItem* item = work_items_.front();
        work_items_.pop_front();
        item->Run();
        delete item;
      }

      if (!TransferPcmDataLocked()) {
        fprintf(stderr, "No ALSA data\n");
        should_sleep = true;
        continue;
      }

      if (has_image_) {
        dropped_image_count_++;
        has_image_ = false;
      }
    }

    bool has_new_image = RenderFrameLocked();
    //fprintf(stderr, "Rendered frame\n");

    {
      Autolock l(lock_);
      if (is_shutting_down_)
        break;

      has_image_ = has_new_image;
      uint64_t now = GetCurrentMillis();
      if (prev_frame_time)
        frame_periods_.push_back(now - prev_frame_time);
      prev_frame_time = now;
      next_render_time += ms_per_frame_;
    }
  }

  delete projectm_;
  DestroyRenderContext();
}

