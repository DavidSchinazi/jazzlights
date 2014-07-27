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

#include <algorithm>

//#include <csignal>

#include "tcl_renderer.h"

// ProjectM can handle 2048 frames per frame. For 15 FPS this translates to
// 30720. Use the next lower standard sampling rate. We request ALSA
// to produce S16_LE, which matches "signed short" used by ProjectM.
static const int kPcmSampleRate = 22050;

Visualizer::Visualizer(
    int width, int height, int texsize, int fps,
    std::string preset_dir, int preset_duration)
    : width_(width), height_(height), texsize_(texsize), fps_(fps),
      alsa_handle_(NULL), projectm_(NULL), projectm_tex_(0),
      total_overrun_count_(0), has_image_(false),
      volume_multiplier_(1), last_volume_rms_(0),
      is_shutting_down_(false), has_started_thread_(false),
      preset_dir_(preset_dir), preset_duration_(preset_duration),
      current_preset_index_(-1),
      lock_(PTHREAD_MUTEX_INITIALIZER) {
  last_render_time_ = GetCurrentMillis();
  ms_per_frame_ = (uint32_t) (1000.0 / (double) fps);

  pcm_buffer_ = new int16_t[PCM::maxsamples * 2];

  image_buffer_size_ = PIX_LEN(texsize_, texsize_);
  image_buffer_ = new uint8_t[image_buffer_size_];

  pcm_samples_per_frame_ = std::min(
      PCM::maxsamples, kPcmSampleRate / fps_);
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

std::string Visualizer::GetCurrentPresetName() {
  Autolock l(lock_);
  return current_preset_;
}

std::string Visualizer::GetCurrentPresetNameProgress() {
  Autolock l(lock_);
  if (current_preset_.empty())
    return "";
  char buf[1024];
  snprintf(buf, sizeof(buf), "(%d/%d) '%s'",
           current_preset_index_ + 1, (int) all_presets_.size(),
           current_preset_.c_str());
  return buf;
}

std::vector<std::string> Visualizer::GetPresetNames() {
  Autolock l(lock_);
  return all_presets_;
}

void Visualizer::SelectNextPreset() {
  Autolock l(lock_);
  ScheduleWorkItemLocked(new NextPresetWorkItem(true));
}

void Visualizer::SelectPreviousPreset() {
  Autolock l(lock_);
  ScheduleWorkItemLocked(new NextPresetWorkItem(false));
}

void Visualizer::ScheduleWorkItemLocked(WorkItem* item) {
  if (is_shutting_down_)
    return;
  work_items_.push_back(item);
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

std::vector<int> Visualizer::GetAndClearFramePeriods() {
  Autolock l(lock_);
  std::vector<int> result = frame_periods_;
  frame_periods_.clear();
  return result;
}

Bytes* Visualizer::GetAndClearLastImageForTest() {
  Autolock l(lock_);
  if (!has_image_)
    return NULL;
  has_image_ = false;
  return new Bytes(image_buffer_, image_buffer_size_);
}

void Visualizer::SetVolumeMultiplier(double value) {
  Autolock l(lock_);
  volume_multiplier_ = value;
}

double Visualizer::GetLastVolumeRms() {
  Autolock l(lock_);
  return last_volume_rms_;
}

void Visualizer::CloseInputLocked() {
  if (alsa_handle_) {
    inp_alsa_cleanup(alsa_handle_);
    alsa_handle_ = NULL;
  }
}

/*static std::string GetPcmDump(int16_t* pcm_buffer, int sample_count) {
  char buf[100 * 1024];
  char* pos = buf;
  for (int i = 0; i < sample_count; i++) {
    pos += sprintf(pos, "%X/%X ", pcm_buffer[i * 2], pcm_buffer[i * 2 + 1]);
  }
  pos[0] = 0;
  return buf;
}*/

static void AdjustVolume(
    int16_t* pcm_buffer, int sample_count, double volume_multiplier) {
  int32_t mult = (int32_t) round(volume_multiplier * 1000.0);
  if (mult == 1000)
    return;
  for (int i = 0; i < sample_count; i++) {
    int32_t s_l = ((int32_t) pcm_buffer[i * 2]) * mult;
    int32_t s_r = ((int32_t) pcm_buffer[i * 2 + 1]) * mult;
    if (s_l >= 32768000) {
      s_l = 32767000;
    } else if (s_l <= -32768000) {
      s_l = -32767000;
    }
    if (s_r >= 32768000) {
      s_r = 32767000;
    } else if (s_r <= -32768000) {
      s_r = -32767000;
    }
    pcm_buffer[i * 2] = (int16_t) (s_l / 1000);
    pcm_buffer[i * 2 + 1] = (int16_t) (s_r / 1000);
  }
}

static double CalcVolumeRms(int16_t* pcm_buffer, int sample_count) {
  double sum_l = 0;
  double sum_r = 0;
  for (int i = 0; i < sample_count; i++) {
    double s_l = ((double) pcm_buffer[i * 2]) / 32768.0;
    double s_r = ((double) pcm_buffer[i * 2 + 1]) / 32768.0;
    sum_l += s_l * s_l;
    sum_r += s_r * s_r;
  }
  sum_l = sqrt(sum_l / sample_count);
  sum_r = sqrt(sum_r / sample_count);
  return (sum_l + sum_r) / 2.0;
}

bool Visualizer::TransferPcmDataLocked() {
  if (!alsa_handle_) {
    if (alsa_device_.empty()) {
      //fprintf(stderr, "ALSA input is disabled\n");
      return false;
    }
    fprintf(stderr, "Connecting to ALSA input %s\n", alsa_device_.c_str());
    alsa_handle_ = inp_alsa_init(alsa_device_.c_str(), kPcmSampleRate);
    if (!alsa_handle_) {
      fprintf(stderr, "Failed to open ALSA input\n");
      return false;
    }
  }

  int sample_count = 0;
  PCM* pcm = projectm_->pcm();
  while (sample_count < pcm_samples_per_frame_) {
    int overrun_count = 0;
    int samples = inp_alsa_read(
        alsa_handle_, pcm_buffer_ + sample_count * 2,
        pcm_samples_per_frame_ - sample_count, &overrun_count);
    total_overrun_count_ += overrun_count;
    if (samples <= 0)
      break;
    sample_count += samples;
  }

  AdjustVolume(pcm_buffer_, sample_count, volume_multiplier_);

  last_volume_rms_ = CalcVolumeRms(pcm_buffer_, sample_count);

  //fprintf(stderr, "Adding %d samples, RMS=%.3f\n",
  //    sample_count, last_volume_rms_);
  //fprintf(stderr, "Adding %d samples %s\n",
  //        sample_count, GetPcmDump(pcm_buffer_, sample_count).c_str());

  bool has_real_data = false;
  for (int i = 0; i < sample_count * 2; i++) {
    if (pcm_buffer_[i]) {
      has_real_data = true;
      break;
    }
  }
  if (sample_count && !has_real_data) {
    fprintf(stderr, "ALSA produced %d samples with empy data\n", sample_count);
  }

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

  static int kVisualAttribs[] = {
      GLX_RENDER_TYPE, GLX_RGBA_BIT,
      GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
      GLX_RED_SIZE, 8,
      GLX_GREEN_SIZE, 8,
      GLX_BLUE_SIZE, 8,
      None
  };
  int fb_config_count = 0;
  GLXFBConfig* fb_configs = glXChooseFBConfig(
      display_, DefaultScreen(display_), kVisualAttribs, &fb_config_count);
  if (!fb_configs || fb_config_count == 0) {
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
  settings.textureSize = texsize_;
  settings.meshX = width_;
  settings.meshY = height_;
  settings.presetURL = preset_dir_;
  settings.titleFontURL = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  settings.menuFontURL = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
  settings.beatSensitivity = 50;
  settings.aspectCorrection = 1;
  // Preset duration is based on gaussian distribution
  // with mean of |presetDuration| and sigma of |easterEgg|.
  settings.presetDuration = preset_duration_;
  settings.easterEgg = 1;
  // Transition period for switching between presets.
  settings.smoothPresetDuration = 3;
  settings.shuffleEnabled = 0;
  settings.softCutRatingsEnabled = 0;
  projectm_ = new projectM(settings);

  projectm_tex_ = projectm_->initRenderToTexture();
  if (projectm_tex_ == -1 || projectm_tex_ == 0) {
    fprintf(stderr, "Unable to init ProjectM texture rendering");
    CHECK(false);
  }

  {
    Autolock l(lock_);
    for (unsigned int i = 0; i < projectm_->getPlaylistSize(); i++) {
      all_presets_.push_back(projectm_->getPresetName(i));
    }
  }

  projectm_->selectPreset(0);
}

// dfplayer/presets/Geiss and Rovastar - The Chaos Of Colours (sprouting dimentia mix).milk
// dfplayer/presets/Unchained - Morat's Final Voyage.milk
// dfplayer/presets/Fvese - Lifesavor Anyone.milk
// dfplayer/presets/Unchained - Beat Demo 2.0.milk
// dfplayer/presets/Rovastar - The Chaos Of Colours.milk
// dfplayer/presets/nil - Can't Stop the Blithering.milk


bool Visualizer::RenderFrameLocked() {
  projectm_->renderFrame();

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    fprintf(stderr, "ProjectM rendering ended with err=0x%x\n", err);
    return true;
  }

  //glFlush();
  glXSwapBuffers(display_, pbuffer_);

  //glReadBuffer(GL_BACK);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, projectm_tex_);

  int w = 0;
  int h = 0;
  int red_size = -1;
  int green_size = -1;
  int blue_size = -1;
  int alpha_size = -1;
  int internal_format = 0;
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_RED_SIZE, &red_size);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_GREEN_SIZE, &green_size);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_BLUE_SIZE, &blue_size);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_ALPHA_SIZE, &alpha_size);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_COMPONENTS, &internal_format);
  if (w != texsize_ || h != texsize_) {
    fprintf(stderr, "Unexpected texture size of %d x %d, instead of %d",
            w, h, texsize_);
    glBindTexture(GL_TEXTURE_2D, 0);
    return false;
  }
  if (red_size != 8 || green_size != 8 || blue_size != 8 ||
      alpha_size != 0 || internal_format != GL_RGB) {
    fprintf(stderr, "Unexpected color sizes of %d %d %d %d fmt=0x%x",
            red_size, green_size, blue_size, alpha_size, internal_format);
    glBindTexture(GL_TEXTURE_2D, 0);
    return false;
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_buffer_);

  err = glGetError();
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_TEXTURE_2D);
  if (err == GL_NO_ERROR) {
    return true;
  }

  // RenderTarget constructor stores:
  //  - FB in fbuffer[0]
  //  - Depth RB in depthb[0]
  //  - FB-bound texture in textureID[0] square size of texsize, RGB
  //  - Another texture in textureID[1] square size of texsize, RGB
  //
  // RenderTarget::lock() binds fbuffer[0]
  // RenderTarget::lock() copies FB into textureID[1] and unbinds FB,
  //   tex remains bound
  //
  // initRenderToTexture() stores:
  //  - FB in fbuffer[1]
  //  - Depth RB in depthb[1]
  //  - FB-bound texture in textureID[2], RGB
  //  - renderToTexture is set to 1

  GLenum status = 0; //glCheckFramebufferStatus(GL_FRAMEBUFFER);
  fprintf(stderr, "Unable to read pixels, err=0x%x, fb_status=0x%x\n",
          err, status);
  return false;
}

void Visualizer::PostTclFrameLocked() {
  TclRenderer* tcl = TclRenderer::GetByControllerId(1);
  if (!tcl) {
    fprintf(stderr, "TCL controller not found\n");
    return;
  }

  int dst_w = tcl->GetWidth();
  int src_w = dst_w / 2;
  int height = tcl->GetHeight();

  uint8_t* src_img1 = ResizeImage(
      image_buffer_, texsize_, texsize_, src_w, height);
  uint8_t* src_img2 = FlipImage(src_img1, src_w, height);

  int len = PIX_LEN(dst_w, height);
  uint8_t* dst = new uint8_t[len];
  PasteSubImage(src_img1, src_w, height,
      dst, 0, 0, dst_w, height, false);
  PasteSubImage(src_img2, src_w, height,
      dst, src_w, 0, dst_w, height, false);

  delete[] src_img1;
  delete[] src_img2;

  AdjustableTime now;
  Bytes* bytes = new Bytes(dst, len);
  tcl->ScheduleImageAt(bytes, 0, now);
  delete[] dst;
  delete bytes;
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

void Visualizer::NextPresetWorkItem::Run(Visualizer* self) {
  unsigned int idx = -1;
  if (!self->projectm_->selectedPresetIndex(idx)) {
    self->projectm_->selectPreset(0);
    return;
  }
  unsigned int size = self->projectm_->getPlaylistSize();
  if (is_next_) {
    idx = (idx < size - 1 ? idx + 1 : 0);
  } else {
    idx = (idx > 0 ? idx - 1 : size - 1);
  }
  self->projectm_->selectPreset(idx);
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
      Sleep(0.2);
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
        item->Run(this);
        delete item;
      }

      if (!TransferPcmDataLocked()) {
        //fprintf(stderr, "No ALSA data\n");
        should_sleep = true;
        continue;
      }
    }

    bool has_new_image = RenderFrameLocked();
    //fprintf(stderr, "Rendered frame = %d\n", (int)has_new_image);

    {
      Autolock l(lock_);
      if (is_shutting_down_)
        break;

      if (projectm_->selectedPresetIndex(current_preset_index_)) {
        current_preset_ = projectm_->getPresetName(current_preset_index_);
      } else {
        current_preset_index_ = -1;
        current_preset_ = "";
      }

      has_image_ = has_new_image;

      if (has_new_image)
        PostTclFrameLocked();

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

