// Copyright 2014, Igor Chernyshev.
// Licensed under The MIT License
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

// MilkDrop and ProjectM expect 44.1kHz sampling rate. We will discard
// all samples that fall out of MilkDrop's sample window of 512. We request
// ALSA to produce S16_LE, which matches "signed short" used by ProjectM.
static const int kPcmSampleRate = 44100;
static const int kPcmMaxSamples = 512;

Visualizer::Visualizer(
    int width, int height, int texsize, int fps,
    const std::string& preset_dir, const std::string& textures_dir,
    int preset_duration)
    : width_(width), height_(height), texsize_(texsize), fps_(fps * 2),
      alsa_handle_(NULL), projectm_(NULL), projectm_tex_(0),
      total_overrun_count_(0), has_image_(false),
      volume_multiplier_(1), last_volume_rms_(0),
      is_shutting_down_(false), has_started_thread_(false),
      preset_dir_(preset_dir), textures_dir_(textures_dir),
      preset_duration_(preset_duration),
      current_preset_index_(-1),
      lock_(PTHREAD_MUTEX_INITIALIZER) {
  last_render_time_ = GetCurrentMillis();
  ms_per_frame_ = (uint32_t) (1000.0 / (double) fps_);

  image_buffer_size_ = RGBA_LEN(texsize_, texsize_);
  image_buffer_ = new uint8_t[image_buffer_size_];

  for (int i = 0; i < 6; ++i) {
    last_bass_info_.push_back(0);
  }
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
  /*int dst_w = 500;
  int src_w = 250;
  int height = 50;
  uint8_t* src_img1 = ResizeImage(
      image_buffer_, texsize_, texsize_, src_w, height);
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

std::vector<double> Visualizer::GetLastBassInfo() {
  Autolock l(lock_);
  return last_bass_info_;
}

void Visualizer::CloseInputLocked() {
  if (alsa_handle_) {
    inp_alsa_cleanup(alsa_handle_);
    alsa_handle_ = NULL;
  }
}

/*static std::string GetPcmDump(float* pcm_buffer, int sample_count) {
  char buf[100 * 1024];
  char* pos = buf;
  for (int i = 0; i < sample_count; i++) {
    pos += sprintf(pos, "%.3f/%.3f ", pcm_buffer[i * 2], pcm_buffer[i * 2 + 1]);
  }
  pos[0] = 0;
  return buf;
}*/

static void AdjustVolume(
    float* pcm_buffer, int sample_count, float volume_multiplier) {
  if (volume_multiplier == 1.0)
    return;
  for (int i = 0; i < sample_count; i++) {
    float s_l = pcm_buffer[i * 2] * volume_multiplier;
    float s_r = pcm_buffer[i * 2 + 1] * volume_multiplier;
    if (s_l > 1.0) {
      s_l = 1.0;
    } else if (s_l < -1.0) {
      s_l = -1.0;
    }
    if (s_r > 1.0) {
      s_r = 1.0;
    } else if (s_r < -1.0) {
      s_r = -1.0;
    }
    pcm_buffer[i * 2] = s_l;
    pcm_buffer[i * 2 + 1] = s_r;
  }
}

static float CalcVolumeRms(float* pcm_buffer, int sample_count) {
  if (sample_count == 0)
    return 0;
  float sum_l = 0;
  float sum_r = 0;
  for (int i = 0; i < sample_count; i++) {
    float s_l = pcm_buffer[i * 2];
    float s_r = pcm_buffer[i * 2 + 1];
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
  float pcm_buffer[kPcmMaxSamples * 2];
  while (sample_count < kPcmMaxSamples) {
    int16_t read_buf[kPcmMaxSamples * 2];
    int overrun_count = 0;
    int samples = inp_alsa_read(
        alsa_handle_, read_buf, kPcmMaxSamples - sample_count, &overrun_count);
    total_overrun_count_ += overrun_count;
    if (samples <= 0)
      break;
    for (int i = 0; i < samples; i++) {
      float s_l = read_buf[i * 2];
      float s_r = read_buf[i * 2 + 1];
      pcm_buffer[sample_count * 2] = s_l / 32768.0;
      pcm_buffer[sample_count * 2 + 1] = s_r / 32768.0;
      sample_count++;
    }
  }

  // Disacrd all remaining samples, so on next frame we can get fresh data.
  int discard_count = 0;
  while (true) {
    int16_t discard_buf[kPcmMaxSamples * 2];
    int overrun_count = 0;
    int samples = inp_alsa_read(
        alsa_handle_, discard_buf, kPcmMaxSamples, &overrun_count);
    total_overrun_count_ += overrun_count;
    if (samples <= 0)
      break;
    discard_count += samples;
  }

  AdjustVolume(pcm_buffer, sample_count, volume_multiplier_);

  last_volume_rms_ = CalcVolumeRms(pcm_buffer, sample_count);

  //fprintf(stderr, "Adding %d samples, discarded=%d, overrun=%d, RMS=%.3f\n",
  //    sample_count, discard_count, total_overrun_count_, last_volume_rms_);
  //fprintf(stderr, "Adding %d samples, discarded=%d, overrun=%d, data=%s\n",
  //        sample_count, discard_count, total_overrun_count_,
  //        GetPcmDump(pcm_buffer, sample_count).c_str());

  bool has_real_data = false;
  for (int i = 0; i < sample_count * 2; i++) {
    if (fabsf(pcm_buffer[i]) > 0.001) {
      has_real_data = true;
      break;
    }
  }
  if (!sample_count) {
    //fprintf(stderr, "ALSA produced no samples, discarded=%d, overrun=%d\n",
    //        discard_count, total_overrun_count_);
  } else if (!has_real_data) {
    //fprintf(stderr, "ALSA produced %d samples with empy data\n", sample_count);
  }

  PCM* pcm = projectm_->pcm();
  pcm->setPCM(pcm_buffer, sample_count);

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
      GLX_ALPHA_SIZE, 8,
      GLX_DEPTH_SIZE, 8,
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
      GLX_PBUFFER_WIDTH,  texsize_,
      GLX_PBUFFER_HEIGHT, texsize_,
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
  settings.windowWidth = texsize_;
  settings.windowHeight = texsize_ / (width_ / height_);
  settings.fps = fps_;
  settings.textureSize = texsize_;
  settings.meshX = 32;
  settings.meshY = 24;
  settings.presetURL = preset_dir_;
  settings.titleFontURL = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  settings.menuFontURL = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf";
  settings.beatSensitivity = 10;
  settings.aspectCorrection = 1;
  // Preset duration is based on gaussian distribution
  // with mean of |presetDuration| and sigma of |easterEgg|.
  settings.presetDuration = preset_duration_;
  settings.easterEgg = 1;
  // Transition period for switching between presets.
  settings.smoothPresetDuration = 3;
  settings.shuffleEnabled = 0;
  settings.softCutRatingsEnabled = 0;
  projectm_ = new projectM(
      settings, "dfplayer/shaders", textures_dir_, projectM::FLAG_NONE);

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


bool Visualizer::RenderFrame(bool need_image) {
  projectm_->renderFrame();

  Autolock l(lock_);
  if (is_shutting_down_)
    return false;

  double bass = 0;
  double bass_att = 0;
  double mid = 0;
  double mid_att = 0;
  double treb = 0;
  double treb_att = 0;
  projectm_->getBassData(&bass, &bass_att, &mid, &mid_att, &treb, &treb_att);
  last_bass_info_.clear();
  last_bass_info_.push_back(bass);
  last_bass_info_.push_back(bass_att);
  last_bass_info_.push_back(mid);
  last_bass_info_.push_back(mid_att);
  last_bass_info_.push_back(treb);
  last_bass_info_.push_back(treb_att);

  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    fprintf(stderr, "ProjectM rendering ended with err=0x%x\n", err);
    return true;
  }

  if (!need_image)
    return false;

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
    fprintf(stderr, "Unexpected texture size of %d x %d, instead of %d\n",
            w, h, texsize_);
    glBindTexture(GL_TEXTURE_2D, 0);
    return false;
  }
  if (red_size != 8 || green_size != 8 || blue_size != 8 ||
      alpha_size != 0 || internal_format != GL_RGB) {
    fprintf(stderr, "Unexpected color sizes of %d %d %d %d fmt=0x%x\n",
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
  if (err != GL_NO_ERROR) {
    GLenum status = 0; //glCheckFramebufferStatus(GL_FRAMEBUFFER);
    fprintf(stderr, "Unable to read pixels, err=0x%x, fb_status=0x%x\n",
            err, status);
    return false;
  }

  const uint8_t* img2 = FlipImage(image_buffer_, texsize_, texsize_, false);
  memcpy(image_buffer_, img2, image_buffer_size_);
  delete[] img2;

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

  return true;
}

void Visualizer::PostTclFrameLocked() {
  static const int kCropWidth = 4;

  TclRenderer* tcl = TclRenderer::GetInstance();
  if (!tcl) {
    fprintf(stderr, "TCL renderer not found\n");
    return;
  }

  AdjustableTime now;
  Bytes* bytes = new Bytes(image_buffer_, RGBA_LEN(texsize_, texsize_));
  tcl->ScheduleImageAt(
      1, bytes, texsize_, texsize_, EFFECT_MIRROR,
      kCropWidth, kCropWidth,
      texsize_ - kCropWidth * 2, texsize_ - kCropWidth * 2,
      0, now);
  delete bytes;
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
  uint32_t frame_num = 0;
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

    // We run at 2x the max rendering FPS to make it 30FPS,
    // so deliver every other frame.
    bool need_image = (frame_num & 1) == 0;
    bool has_new_image = RenderFrame(need_image);
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

      has_image_ |= has_new_image;

      if (has_new_image)
        PostTclFrameLocked();

      uint64_t now = GetCurrentMillis();
      if (prev_frame_time)
        frame_periods_.push_back(now - prev_frame_time);
      prev_frame_time = now;
      next_render_time += ms_per_frame_;
    }

    frame_num++;
  }

  delete projectm_;
  DestroyRenderContext();
}

