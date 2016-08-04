// Copyright 2014, Igor Chernyshev.

#ifndef TCL_TCL_CONTROLLER_H_
#define TCL_TCL_CONTROLLER_H_

#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "tcl/tcl_types.h"
#include "util/led_layout.h"
#include "util/pixels.h"

class Effect;

class TclController {
 public:
  TclController(
      int id, int width, int height, int fps,
      const LedLayout& layout, double gamma);
  ~TclController();

  int id() const { return id_; }
  int width() const { return width_; }
  int height() const { return height_; }

  InitStatus init_status() const { return init_status_; }
  void MarkInitialized() { init_status_ = INIT_STATUS_OK; }

  std::unique_ptr<RgbaImage> GetAndClearLastImage();
  std::unique_ptr<RgbaImage> GetAndClearLastLedImage();
  int last_image_id() const { return last_image_id_; }

  void StartEffect(Effect* effect, int priority);

  void SetGammaRanges(
      int r_min, int r_max, double r_gamma,
      int g_min, int g_max, double g_gamma,
      int b_min, int b_max, double b_gamma);

  // Socket communication funtions are invoked from worker thread only.
  void UpdateAutoReset(uint64_t auto_reset_after_no_data_ms);
  void ScheduleReset();
  InitStatus InitController();
  bool SendFrame(const uint8_t* frame_data);

  void BuildFrameDataForImage(
      std::vector<uint8_t>* dst, RgbaImage* img, int id, InitStatus* status);

  std::vector<uint8_t> GetFrameDataForTest(const RgbaImage& image);

  void SetHdrMode(HdrMode mode);

  static int GetFrameSendDurationMs();

 private:
  TclController(const TclController& src);
  TclController& operator=(const TclController& rhs);

  struct EffectInfo {
    EffectInfo() : effect(nullptr), priority(0) {}
    EffectInfo(Effect* effect, int priority)
        : effect(effect), priority(priority) {}

    bool operator<(const EffectInfo& other) const {
      return (priority > other.priority);
    }

    Effect* effect;
    int priority;
  };

  using EffectList = std::vector<EffectInfo>;

  bool PopulateLedStrandsColors(
      LedStrands* strands, const RgbaImage& image);
  void SavePixelsForLedStrands(const LedStrands& strands);
  void PerformHdr(LedStrands* strands);
  void ApplyLedStrandsGamma(LedStrands* strands);

  std::unique_ptr<LedStrands> ConvertImageToLedStrands(const RgbaImage& image);
  void ConvertLedStrandsToFrame(
      std::vector<uint8_t>* dst, const LedStrands& strands);
  int BuildFrameColorSeq(
      uint8_t* dst, const LedStrands& strands, int led_id, int color_component);

  bool Connect();
  void CloseSocket();
  bool SendPacket(const void* data, int size);
  void ConsumeReplyData();
  void SetLastReplyTime();

  void ApplyEffectsOnImage(RgbaImage* image);
  void ApplyEffectsOnLeds(LedStrands* strands);

  int id_;
  int width_;
  int height_;
  int fps_;
  // TODO(igorc): Do atomic read.
  volatile InitStatus init_status_ = INIT_STATUS_UNUSED;
  RgbGamma gamma_;
  LedLayout layout_;
  LedLayoutMap layout_map_;
  int socket_ = -1;
  bool require_reset_ = true;
  uint64_t last_reply_time_ = 0;
  uint64_t reset_start_time_ = 0;
  RgbaImage last_image_;
  RgbaImage last_led_pixel_snapshot_;
  int last_image_id_ = 0;
  int frames_sent_after_reply_ = 0;
  HdrMode hdr_mode_ = HDR_MODE_NONE;

  pthread_mutex_t effects_lock_;
  EffectList effects_;
};

#endif  // TCL_TCL_CONTROLLER_H_
