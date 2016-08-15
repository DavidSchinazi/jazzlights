// Copyright 2014, Igor Chernyshev.

#include "tcl/tcl_controller.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <opencv2/opencv.hpp>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <algorithm>

#include "model/effect.h"
#include "util/lock.h"
#include "util/logging.h"
#include "util/time.h"

namespace {

// Controller dimensions: 8 strands with 512 LED's each.
const int kControllerStrandLength = 512;
const int kControllerFrameLength = kControllerStrandLength * 8 * 3;

const int kMgsStartDelayUs = 500;
const int kMgsDataDelayUs = 1500;
const int kFrameSendDurationUs =
    kMgsStartDelayUs + kMgsDataDelayUs * (kControllerFrameLength / 1024);

}  // namespace

TclController::TclController(
    int id, int width, int height, int fps, const LedLayout& layout,
    double gamma)
    : id_(id), width_(width), height_(height), fps_(fps), layout_(layout),
      layout_map_(width, height), effects_lock_(PTHREAD_MUTEX_INITIALIZER) {
  SetGammaRanges(0, 255, gamma, 0, 255, gamma, 0, 255, gamma);
  layout_map_.PopulateLayoutMap(layout_);
}

TclController::~TclController() {
  CloseSocket();
  pthread_mutex_destroy(&effects_lock_);
}

int TclController::GetFrameSendDurationMs() {
  return kFrameSendDurationUs / 1000;
}

void TclController::UpdateAutoReset(uint64_t auto_reset_after_no_data_ms) {
  if (auto_reset_after_no_data_ms <= 0 || require_reset_ ||
      init_status_ == INIT_STATUS_RESETTING || frames_sent_after_reply_ <= 2) {
    return;
  }

  uint64_t reply_delay = GetCurrentMillis() - last_reply_time_;
  if (reply_delay > auto_reset_after_no_data_ms) {
    fprintf(stderr, "No reply in %lld ms and %d frames, RESETTING !!!\n",
            (long long) reply_delay, frames_sent_after_reply_);
    require_reset_ = true;
  }
}

void TclController::ScheduleReset() {
  require_reset_ = true;
}

std::unique_ptr<RgbaImage> TclController::GetAndClearLastImage() {
  return last_image_.CloneAndClear(true);
}

std::unique_ptr<RgbaImage> TclController::GetAndClearLastLedImage() {
  return last_led_pixel_snapshot_.CloneAndClear(true);
}

void TclController::SetGammaRanges(
    int r_min, int r_max, double r_gamma,
    int g_min, int g_max, double g_gamma,
    int b_min, int b_max, double b_gamma) {
  gamma_.SetGammaRanges(
      r_min, r_max, r_gamma, g_min, g_max, g_gamma, b_min, b_max, b_gamma);
}

void TclController::SetHdrMode(HdrMode mode) {
  hdr_mode_ = mode;
}

void TclController::StartEffect(Effect* effect, int priority) {
  Autolock l(effects_lock_);
  effect->Initialize(width_, height_, fps_, layout_);
  effects_.push_back(EffectInfo(effect, priority));
  std::sort(effects_.begin(), effects_.end());
}

void TclController::ApplyEffectsOnImage(RgbaImage* image) {
  Autolock l(effects_lock_);
  for (EffectList::iterator it = effects_.begin(); it != effects_.end(); ) {
    bool is_last = it->effect->IsStopped();
    if (!is_last)
      it->effect->ApplyOnImage(image, &is_last);

    if (is_last) {
      effects_.erase(it++);
    } else {
      ++it;
    }
  }
}

void TclController::ApplyEffectsOnLeds(LedStrands* strands) {
  Autolock l(effects_lock_);
  for (EffectList::iterator it = effects_.begin(); it != effects_.end(); ) {
    bool is_last = it->effect->IsStopped();
    if (!is_last)
      it->effect->ApplyOnLeds(strands, &is_last);

    if (is_last) {
      effects_.erase(it++);
    } else {
      ++it;
    }
  }
}

void TclController::BuildFrameDataForImage(
    std::vector<uint8_t>* dst, RgbaImage* image, int id, InitStatus* status) {
  dst->clear();
  *status = init_status_;

  ApplyEffectsOnImage(image);

  std::unique_ptr<LedStrands> strands = ConvertImageToLedStrands(*image);
  if (!strands)
    return;

  ConvertLedStrandsToFrame(dst, *strands.get());

  last_image_id_ = id;
  last_image_ = *image;
  EraseAlpha(last_image_.data(), width_, height_);

  // Only show when OK, to make reset status more obvious.
  if (init_status_ == INIT_STATUS_OK)
    SavePixelsForLedStrands(*strands.get());
}

std::unique_ptr<LedStrands> TclController::ConvertImageToLedStrands(
    const RgbaImage& image) {
  std::unique_ptr<LedStrands> strands(new LedStrands(layout_map_));
  if (!PopulateLedStrandsColors(strands.get(), image))
    return nullptr;

  strands->ConvertTo(LedStrands::TYPE_HSL);

  PerformHdr(strands.get());

  // TODO(igorc): Adjust S and L through a user-controlled gamma curve.

  // TODO(igorc): Fill the darkness.

  ApplyEffectsOnLeds(strands.get());

  strands->ConvertTo(LedStrands::TYPE_RGB);

  // Apply gamma at the end to preserve linear RGB-HSL conversions.
  ApplyLedStrandsGamma(strands.get());
  return strands;
}

std::vector<uint8_t> TclController::GetFrameDataForTest(
    const RgbaImage& image) {
  std::vector<uint8_t> result;
  if (image.empty())
    return result;
  std::unique_ptr<LedStrands> strands = ConvertImageToLedStrands(image);
  if (!strands)
    return result;
  ConvertLedStrandsToFrame(&result, *strands.get());
  return result;
}

bool TclController::PopulateLedStrandsColors(
    LedStrands* strands, const RgbaImage& image) {
  const uint8_t* image_data = image.data();
  int image_len = RGBA_LEN(width_, height_);
  for (int strand_id  = 0; strand_id < strands->GetStrandCount(); ++strand_id) {
    int strand_len = strands->GetLedCount(strand_id);
    uint8_t* dst_colors = strands->GetColorData(strand_id);
    for (int led_id = 0; led_id < strand_len; ++led_id) {
      const std::vector<LedCoord> coords =
          layout_map_.GetLedCoords(strand_id, led_id);
      uint32_t coord_count = coords.size();
      if (!coord_count)
        continue;

      uint32_t r1 = 0, g1 = 0, b1 = 0, c1 = 0;
      uint32_t r2 = 0, g2 = 0, b2 = 0, c2 = 0;
      for (uint32_t c_id = 0; c_id < coord_count; ++c_id) {
        int x = coords[c_id].x;
        int y = coords[c_id].y;
        int color_idx = (y * width_ + x) * 4;
        if (color_idx < 0 || (color_idx + 4) > image_len) {
          fprintf(stderr,
                  "Not enough data in image. Accessing %d, len=%d, strand=%d, "
                  "led=%d, x=%d, y=%d\n",
                  color_idx, image_len, strand_id, led_id, x, y);
          return false;
        }
        if (image_data[color_idx + 3] == 255) {
          r1 += image_data[color_idx];
          g1 += image_data[color_idx + 1];
          b1 += image_data[color_idx + 2];
          c1++;
        } else {
          r2 += image_data[color_idx];
          g2 += image_data[color_idx + 1];
          b2 += image_data[color_idx + 2];
          c2++;
        }
      }

      if (c1 && c1 >= coord_count / 2) {
        r1 = (r1 / c1) & 0xFF;
        g1 = (g1 / c1) & 0xFF;
        b1 = (b1 / c1) & 0xFF;
      } else if (c2) {
        r1 = (r2 / c2) & 0xFF;
        g1 = (g2 / c2) & 0xFF;
        b1 = (b2 / c2) & 0xFF;
      } else {
        r1 = 0;
        g1 = 0;
        b1 = 0;
      }

      uint32_t color = PACK_COLOR32(r1, g1, b1, 255);

      int dst_idx = led_id * 4;
      *((uint32_t*) (dst_colors + dst_idx)) = color;
    }
  }
  return true;
}

void TclController::ApplyLedStrandsGamma(LedStrands* strands) {
  uint8_t* all_colors = strands->GetAllColorData();
  for (int i = 0; i < strands->GetTotalLedCount(); ++i) {
    uint32_t* color = (uint32_t*) (all_colors + i * 4);
    *color = gamma_.Apply(*color);
  }
}

void TclController::SavePixelsForLedStrands(const LedStrands& strands) {
  uint8_t* led_image_data = new uint8_t[RGBA_LEN(width_, height_)];
  memset(led_image_data, 0, RGBA_LEN(width_, height_));
  for (int strand_id  = 0; strand_id < strands.GetStrandCount(); ++strand_id) {
    int strand_len = strands.GetLedCount(strand_id);
    const uint8_t* colors = strands.GetColorData(strand_id);
    for (int led_id = 0; led_id < strand_len; ++led_id) {
      const std::vector<LedCoord>& coords =
          layout_map_.GetLedCoords(strand_id, led_id);
      uint32_t color = *((uint32_t*) (colors + led_id * 4));
      for (size_t c_id = 0; c_id < coords.size(); ++c_id) {
        int x = coords[c_id].x;
        int y = coords[c_id].y;
        int color_idx = (y * width_ + x) * 4;
        *((uint32_t*) (led_image_data + color_idx)) = color;
      }
    }
  }
  last_led_pixel_snapshot_.Set(led_image_data, width_, height_);
  delete[] led_image_data;
}

// Extrapolates value to a range from 0 to 255.
#define EXTEND256(value, min, max)   \
  ((max) == (min) ? (max) :          \
      ((255 * ((value) - (min)) / ((max) - (min))) & 0xFF))

void TclController::PerformHdr(LedStrands* strands) {
  if (hdr_mode_ == HDR_MODE_NONE)
    return;

  uint8_t* res_colors[strands->GetStrandCount()];
  for (int i = 0; i < strands->GetStrandCount(); ++i) {
    res_colors[i] = new uint8_t[strands->GetLedCount(i) * 4];
  }

  // Populate resulting colors with HDR image.
  for (int strand_id  = 0; strand_id < strands->GetStrandCount(); ++strand_id) {
    int strand_len = strands->GetLedCount(strand_id);
    const uint8_t* src_strand_colors = strands->GetColorData(strand_id);
    for (int led_id = 0; led_id < strand_len; ++led_id) {
      uint32_t l_min = 255, l_max = 0, s_min = 255, s_max = 0;
      const std::vector<LedAddress>& siblings =
          layout_map_.GetHdrSiblings(strand_id, led_id);
      size_t siblings_size = siblings.size();
      const LedAddress* siblings_data = siblings.data();
      const uint8_t* src_color = src_strand_colors + led_id * 4;
      uint8_t* res_color = res_colors[strand_id] + led_id * 4;
      for (size_t i = 0; i < siblings_size; ++i) {
        uint8_t* hls = strands->GetColorData(siblings_data[i].strand_id) +
            siblings_data[i].led_id * 4;
        uint8_t l = hls[1];
        uint8_t s = hls[2];
        if (l < l_min) {
          l_min = l;
        }
        if (l > l_max) {
          l_max = l;
        }
        if (s < s_min) {
          s_min = s;
        }
        if (s > s_max) {
          s_max = s;
        }
      }
      // Always preserve the hue and alpha.
      res_color[0] = src_color[0];
      if (hdr_mode_ == HDR_MODE_LUM_SAT ||
	  hdr_mode_ == HDR_MODE_LUM) {
        res_color[1] = EXTEND256(src_color[1], l_min, l_max);
      } else {
        res_color[1] = src_color[1];
      }
      if (hdr_mode_ == HDR_MODE_LUM_SAT ||
	  hdr_mode_ == HDR_MODE_SAT) {
        res_color[2] = EXTEND256(src_color[2], s_min, s_max);
      } else {
        res_color[2] = src_color[2];
      }
      res_color[3] = src_color[3];
    }
  }

  // Copy resulting colors back to the strand.
  for (int strand_id  = 0; strand_id < strands->GetStrandCount(); ++strand_id) {
    int strand_len = strands->GetLedCount(strand_id);
    memcpy(strands->GetColorData(strand_id), res_colors[strand_id],
           strand_len * 4);
  }

  for (int i = 0; i < strands->GetStrandCount(); ++i)
    delete[] res_colors[i];
}

void TclController::ConvertLedStrandsToFrame(
    std::vector<uint8_t>* dst, const LedStrands& strands) {
  dst->resize(kControllerFrameLength);
  int pos = 0;
  for (int led_id = 0; led_id < kControllerStrandLength; led_id++) {
    pos += BuildFrameColorSeq(&(*dst)[pos], strands, led_id, 2);
    pos += BuildFrameColorSeq(&(*dst)[pos], strands, led_id, 1);
    pos += BuildFrameColorSeq(&(*dst)[pos], strands, led_id, 0);
  }
  CHECK(pos == kControllerFrameLength);
  for (int i = 0; i < kControllerFrameLength; i++) {
    // Black color is offset by 0x2C.
    (*dst)[i] = ((*dst)[i] + 0x2C) & 0xFF;
  }
}

int TclController::BuildFrameColorSeq(
    uint8_t* dst, const LedStrands& strands, int led_id, int color_component) {
  int pos = 0;
  int color_bit_mask = 0x80;
  while (color_bit_mask > 0) {
    uint8_t dst_byte = 0;
    for (int strand_id  = 0; strand_id < strands.GetStrandCount();
	 strand_id++) {
      if (led_id >= strands.GetLedCount(strand_id))
        continue;
      const uint8_t* colors = strands.GetColorData(strand_id);
      uint8_t color = colors[led_id * 4 + color_component];
      if ((color & color_bit_mask) != 0)
        dst_byte |= 1 << strand_id;
    }
    color_bit_mask >>= 1;
    dst[pos++] = dst_byte;
  }
  CHECK(pos == 8);
  return pos;
}

void TclController::CloseSocket() {
  if (socket_ != -1) {
    close(socket_);
    socket_ = -1;
  }
}

bool TclController::Connect() {
  if (socket_ != -1)
    return true;

  socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ == -1) {
    REPORT_ERRNO("socket");
    return false;
  }

  /*if (fcntl(socket_, F_SETFL, O_NONBLOCK, 1) == -1) {
    REPORT_ERRNO("fcntl");
    CloseSocket();
    return false;
  }*/

  struct sockaddr_in si_local;
  memset(&si_local, 0, sizeof(si_local));
  si_local.sin_family = AF_INET;
  si_local.sin_port = htons(0);
  si_local.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(socket_, (struct sockaddr*) &si_local, sizeof(si_local)) == -1) {
    REPORT_ERRNO("bind");
    CloseSocket();
    return false;
  }

  char addr_str[65];
  snprintf(addr_str, sizeof(addr_str), "192.168.60.%d", (49 + id_));

  struct sockaddr_in si_remote;
  memset(&si_remote, 0, sizeof(si_remote));
  si_remote.sin_family = AF_INET;
  si_remote.sin_port = htons(5000);
  if (inet_aton(addr_str, &si_remote.sin_addr) == 0) {
    fprintf(stderr, "inet_aton() failed\n");
    CloseSocket();
    return false;
  }

  if (TEMP_FAILURE_RETRY(connect(
          socket_, (struct sockaddr*) &si_remote, sizeof(si_remote))) == -1) {
    REPORT_ERRNO("connect");
    CloseSocket();
    return false;
  }

  return true;
}

bool TclController::SendPacket(const void* data, int size) {
  int sent = TEMP_FAILURE_RETRY(send(socket_, data, size, 0));
  if (sent == -1) {
    REPORT_ERRNO("send");
    return false;
  }
  if (sent != size) {
    fprintf(stderr, "Not all data was sent in UDP packet\n");
    require_reset_ = true;
    return false;
  }
  return true;
}

InitStatus TclController::InitController() {
  static const uint8_t MSG_INIT[] = {0xC5, 0x77, 0x88, 0x00, 0x00};
  static const double MSG_INIT_DELAY = 0.1;
  static const uint8_t MSG_RESET[] = {0xC2, 0x77, 0x88, 0x00, 0x00};
  static const uint64_t MSG_RESET_DELAY_MS = 5000;

  if (!Connect()) {
    init_status_ = INIT_STATUS_FAIL;
    return init_status_;
  }

  if (init_status_ == INIT_STATUS_RESETTING) {
    require_reset_ = false;
    uint64_t duration = GetCurrentMillis() - reset_start_time_;
    if (duration < MSG_RESET_DELAY_MS)
      return init_status_;
    fprintf(stderr, "Completed a requested reset\n");
    init_status_ = INIT_STATUS_UNUSED;
    reset_start_time_ = 0;
  }

  if (require_reset_) {
    fprintf(stderr, "Performing a requested reset\n");
    if (!SendPacket(MSG_RESET, sizeof(MSG_RESET))) {
      init_status_ = INIT_STATUS_FAIL;
      return init_status_;
    }
    require_reset_ = false;
    init_status_ = INIT_STATUS_RESETTING;
    reset_start_time_ = GetCurrentMillis();
    return init_status_;
  }

  if (init_status_ == INIT_STATUS_OK)
    return init_status_;

  // Either kUnused or kFail. Init again.

  if (!SendPacket(MSG_INIT, sizeof(MSG_INIT))) {
    init_status_ = INIT_STATUS_FAIL;
    return init_status_;
  }
  Sleep(MSG_INIT_DELAY);

  init_status_ = INIT_STATUS_OK;
  SetLastReplyTime();
  return init_status_;
}

bool TclController::SendFrame(const uint8_t* frame_data) {
  static const uint8_t MSG_START_FRAME[] = {0xC5, 0x77, 0x88, 0x00, 0x00};
  static const uint8_t MSG_END_FRAME[] = {0xAA, 0x01, 0x8C, 0x01, 0x55};
  static const uint8_t FRAME_MSG_PREFIX[] = {
      0x88, 0x00, 0x68, 0x3F, 0x2B, 0xFD,
      0x60, 0x8B, 0x95, 0xEF, 0x04, 0x69};
  static const uint8_t FRAME_MSG_SUFFIX[] = {0x00, 0x00, 0x00, 0x00};

  // uint64_t start_time = GetCurrentMillis();
  // int total_delay = kMgsStartDelayUs;
  ConsumeReplyData();
  if (!SendPacket(MSG_START_FRAME, sizeof(MSG_START_FRAME)))
    return false;
  SleepUs(kMgsStartDelayUs);

  uint8_t packet[sizeof(FRAME_MSG_PREFIX) + 1024 + sizeof(FRAME_MSG_SUFFIX)];
  memcpy(packet, FRAME_MSG_PREFIX, sizeof(FRAME_MSG_PREFIX));
  memcpy(packet + sizeof(FRAME_MSG_PREFIX) + 1024,
         FRAME_MSG_SUFFIX, sizeof(FRAME_MSG_SUFFIX));

  int message_idx = 0;
  int frame_data_pos = 0;
  while (frame_data_pos < kControllerFrameLength) {
    packet[1] = message_idx++;
    memcpy(packet + sizeof(FRAME_MSG_PREFIX),
           frame_data + frame_data_pos, 1024);
    frame_data_pos += 1024;

    if (!SendPacket(packet, sizeof(packet)))
      return false;
    SleepUs(kMgsDataDelayUs);
    // total_delay += kMgsDataDelayUs;
  }
  CHECK(frame_data_pos == kControllerFrameLength);
  CHECK(message_idx == 12);

  if (!SendPacket(MSG_END_FRAME, sizeof(MSG_END_FRAME)))
    return false;
  ConsumeReplyData();
  frames_sent_after_reply_++;
  // uint64_t delay = GetCurrentMillis() - start_time;
  // fprintf(stderr, "Wanted to wait for %d, waited for %d\n",
  //         total_delay, (int) delay * 1000);
  return true;
}

void TclController::ConsumeReplyData() {
  // static const uint8_t MSG_REPLY[] = {0x55, 0x00, 0x00, 0x00, 0x00};
  uint8_t buf[65536];
  while (true) {
    ssize_t size = TEMP_FAILURE_RETRY(
        recv(socket_, buf, sizeof(buf), MSG_DONTWAIT));
    if (size != -1) {
      SetLastReplyTime();
      continue;
    }
    if (errno != EAGAIN && errno != EWOULDBLOCK)
      REPORT_ERRNO("recv");
    break;
  }
}

void TclController::SetLastReplyTime() {
  last_reply_time_ = GetCurrentMillis();
  frames_sent_after_reply_ = 0;
}
