// Copyright 2014, Igor Chernyshev.
// Licensed under Apache 2.0 ASF license.
//
// TCL controller access module.

#include "tcl_renderer.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define FRAME_DATA_LEN  (STRAND_LENGTH * 8 * 3)

#define REPORT_ERRNO(name)                           \
  fprintf(stderr, "Failure in '%s' call: %d, %s\n",  \
          name, errno, strerror(errno));

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CHECK(cond)                                             \
  if (!(cond)) {                                                \
    fprintf(stderr, "EXITING with check-fail at %s (%s:%d)"     \
            ". Condition = '" TOSTRING(cond) "'\n",             \
            __FILE__, __FUNCTION__, __LINE__);                  \
    exit(-1);                                                   \
  }


class Autolock {
 public:
  Autolock(pthread_mutex_t& lock) : lock_(&lock) {
    int err = pthread_mutex_lock(lock_);
    if (err != 0) {
      fprintf(stderr, "Unable to aquire mutex: %d\n", err);
      CHECK(false);
    }
  }

  ~Autolock() {
    int err = pthread_mutex_unlock(lock_);
    if (err != 0) {
      fprintf(stderr, "Unable to release mutex: %d\n", err);
      CHECK(false);
    }
  }

 private:
  Autolock(const Autolock& src);
  Autolock& operator=(const Autolock& rhs);

  pthread_mutex_t* lock_;
};

static uint64_t GetCurrentMillis() {
  struct timespec time;
  if (clock_gettime(CLOCK_MONOTONIC, &time) == -1) {
    REPORT_ERRNO("clock_gettime(monotonic)");
    CHECK(false);
  }
  return ((uint64_t) time.tv_sec) * 1000 + time.tv_nsec / 1000000;
}

Time::Time() : time_(GetCurrentMillis()) {}

void Time::AddMillis(int ms) {
  time_ += ms;
}

Layout::Layout() {
  memset(lengths_, 0, sizeof(lengths_));
}

void Layout::AddCoord(int strand_id, int x, int y) {
  CHECK(strand_id >= 0 && strand_id < STRAND_COUNT);
  if (lengths_[strand_id] == STRAND_LENGTH) {
    fprintf(stderr, "Cannot add more coords to strand %d\n", strand_id);
    return;
  }
  int pos = lengths_[strand_id];
  lengths_[strand_id] = pos + 1;
  x_[strand_id][pos] = x;
  y_[strand_id][pos] = y;
}

Bytes::Bytes(void* data, int len)
    : data_(reinterpret_cast<uint8_t*>(data)), len_(len) {}

Bytes::~Bytes() {
  if (data_)
    delete[] data_;
}

TclRenderer::TclRenderer(
    int controller_id, int width, int height,
    const Layout& layout, double gamma)
    : controller_id_(controller_id), width_(width), height_(height),
      mgs_start_delay_us_(500), mgs_data_delay_us_(1500),
      auto_reset_after_no_data_ms_(5000), init_sent_(false),
      is_shutting_down_(false), has_started_thread_(false), layout_(layout),
      socket_(-1), require_reset_(true), last_reply_time_(0),
      lock_(PTHREAD_MUTEX_INITIALIZER),
      cond_(PTHREAD_COND_INITIALIZER),
      frames_sent_after_reply_(0) {
  SetGamma(gamma);
}

TclRenderer::~TclRenderer() {
  if (has_started_thread_) {
    {
      Autolock l(lock_);
      is_shutting_down_ = true;
      pthread_cond_broadcast(&cond_);
    }

    pthread_join(thread_, NULL);
  }

  while (!queue_.empty()) {
    WorkItem item = queue_.top();
    queue_.pop();
    if (item.frame_data_)
      delete[] item.frame_data_;
  }
  pthread_cond_destroy(&cond_);
  pthread_mutex_destroy(&lock_);
}

void TclRenderer::StartMessageLoop() {
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
  if (r_min < 0 || r_max > 255 || r_min > r_max || r_gamma < 0.1 ||
      g_min < 0 || g_max > 255 || g_min > g_max || g_gamma < 0.1 ||
      b_min < 0 || b_max > 255 || b_min > b_max || b_gamma < 0.1) {
    fprintf(stderr, "Incorrect gamma values\n");
    return;
  }

  Autolock l(lock_);
  for (int i = 0; i < 256; i++) {
    double d = ((double) i) / 255.0;
    gamma_r_[i] =
        (int) (r_min + floor((r_max - r_min) * pow(d, r_gamma) + 0.5));
    gamma_g_[i] =
        (int) (g_min + floor((g_max - g_min) * pow(d, g_gamma) + 0.5));
    gamma_b_[i] =
        (int) (b_min + floor((b_max - b_min) * pow(d, b_gamma) + 0.5));
  }
}

void TclRenderer::SetAutoResetAfterNoDataMs(int value) {
  Autolock l(lock_);
  auto_reset_after_no_data_ms_ = value;
}

std::vector<int> TclRenderer::GetAndClearFrameDelays() {
  Autolock l(lock_);
  std::vector<int> result = frame_delays_;
  frame_delays_.clear();
  return result;
}

int TclRenderer::GetFrameSendDuration() {
  Autolock l(lock_);
  int usec = mgs_start_delay_us_ + mgs_data_delay_us_ * (FRAME_DATA_LEN / 1024);
  return usec / 1000;
}

void TclRenderer::ScheduleImageAt(Bytes* bytes, const Time& time) {
  Strands* strands = ConvertImageToStrands(bytes->data_, bytes->len_);
  if (!strands)
    return;
  ApplyGamma(strands);
  ScheduleStrandsAt(strands, time.time_);
  delete strands;
}

void TclRenderer::ScheduleStrandsAt(Strands* strands, uint64_t time) {
  uint8_t* frame_data = ConvertStrandsToFrame(strands);

  Autolock l(lock_);
  CHECK(has_started_thread_);
  if (is_shutting_down_) {
    delete[] frame_data;
    return;
  }
  // TODO(igorc): Recalculate time from relative to absolute.
  uint64_t abs_time = time;
  queue_.push(WorkItem(false, frame_data, abs_time));
  int err = pthread_cond_broadcast(&cond_);
  if (err != 0) {
    fprintf(stderr, "Unable to signal condition: %d\n", err);
    CHECK(false);
  }
}

std::vector<int> TclRenderer::GetFrameDataForTest(Bytes* bytes) {
  std::vector<int> result;
  Strands* strands = ConvertImageToStrands(bytes->data_, bytes->len_);
  if (!strands)
    return result;
  ApplyGamma(strands);
  uint8_t* frame_data = ConvertStrandsToFrame(strands);
  for (int i = 0; i < FRAME_DATA_LEN; i++) {
    result.push_back(frame_data[i]);
  }
  delete[] frame_data;
  delete strands;
  return result;
}

int TclRenderer::GetQueueSize() {
  Autolock l(lock_);
  return queue_.size();
}

TclRenderer::Strands* TclRenderer::ConvertImageToStrands(
    uint8_t* image_data, int len) {
  Autolock l(lock_);
  Strands* strands = new Strands();
  for (int strand_id  = 0; strand_id < STRAND_COUNT; strand_id++) {
    int* x = layout_.x_[strand_id];
    int* y = layout_.y_[strand_id];
    int strand_len = layout_.lengths_[strand_id];
    uint8_t* dst_colors = strands->colors[strand_id];
    for (int i = 0; i < strand_len; i++) {
      int color_idx = (y[i] * width_ + x[i]) * 3;
      if ((color_idx + 3) > len) {
        fprintf(stderr,
                "Not enough data in image. Accessing %d, len=%d, strand=%d, "
                "led=%d, x=%d, y=%d\n",
                color_idx, len, strand_id, i, x[i], y[i]);
        delete strands;
        return NULL;
      }
      int dst_idx = i * 3;
      dst_colors[dst_idx] = image_data[color_idx];
      dst_colors[dst_idx + 1] = image_data[color_idx + 1];
      dst_colors[dst_idx + 2] = image_data[color_idx + 2];
    }
    strands->lengths[strand_id] = strand_len;
  }
  return strands;
}

void TclRenderer::ApplyGamma(TclRenderer::Strands* strands) {
  Autolock l(lock_);
  for (int strand_id  = 0; strand_id < STRAND_COUNT; strand_id++) {
    int len = strands->lengths[strand_id] * 3;
    uint8_t* colors = strands->colors[strand_id];
    for (int i = 0; i < len; i += 3) {
      colors[i] = gamma_r_[colors[i]];
      colors[i + 1] = gamma_g_[colors[i + 1]];
      colors[i + 2] = gamma_b_[colors[i + 2]];
    }
  }
}

// static
uint8_t* TclRenderer::ConvertStrandsToFrame(TclRenderer::Strands* strands) {
  uint8_t* result = new uint8_t[FRAME_DATA_LEN];
  int pos = 0;
  for (int led_id = 0; led_id < STRAND_LENGTH; led_id++) {
    pos += BuildFrameColorSeq(strands, led_id, 2, result + pos);
    pos += BuildFrameColorSeq(strands, led_id, 1, result + pos);
    pos += BuildFrameColorSeq(strands, led_id, 0, result + pos);
  }
  CHECK(pos == FRAME_DATA_LEN);
  for (int i = 0; i < FRAME_DATA_LEN; i++) {
    // Black color is offset by 0x2C.
    result[i] = (result[i] + 0x2C) & 0xFF;
  }
  return result;
}

// static
int TclRenderer::BuildFrameColorSeq(
    TclRenderer::Strands* strands, int led_id,
    int color_component, uint8_t* dst) {
  int pos = 0;
  int color_bit_mask = 0x80;
  while (color_bit_mask > 0) {
    uint8_t dst_byte = 0;
    for (int strand_id  = 0; strand_id < STRAND_COUNT; strand_id++) {
      if (led_id >= strands->lengths[strand_id])
        continue;
      uint8_t* colors = strands->colors[strand_id];
      uint8_t color = colors[led_id * 3 + color_component];
      if ((color & color_bit_mask) != 0)
        dst_byte |= 1 << strand_id;
    }
    color_bit_mask >>= 1;
    dst[pos++] = dst_byte;
  }
  CHECK(pos == 8);
  return pos;
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
void* TclRenderer::ThreadEntry(void* arg) {
  TclRenderer* self = reinterpret_cast<TclRenderer*>(arg);
  self->Run();
  return NULL;
}

void TclRenderer::Run() {
  while (true) {
    uint64_t auto_reset_after_no_data_ms;
    {
      Autolock l(lock_);
      if (is_shutting_down_)
        break;
      auto_reset_after_no_data_ms = auto_reset_after_no_data_ms_;
    }

    if (!require_reset_ && frames_sent_after_reply_ > 2 &&
        auto_reset_after_no_data_ms > 0) {
      uint64_t reply_delay = GetCurrentMillis() - last_reply_time_;
      if (reply_delay > auto_reset_after_no_data_ms) {
        fprintf(stderr, "No reply in %ld ms and %d frames, RESETTING !!!\n",
                reply_delay, frames_sent_after_reply_);
        require_reset_ = true;
      }
    }

    if (!InitController()) {
      Sleep(1);
      continue;
    }

    WorkItem item(false, NULL, 0);
    {
      Autolock l(lock_);
      while (!is_shutting_down_) {
        int64_t next_time;
        if (PopNextWorkItemLocked(&item, &next_time))
          break;
        WaitForQueueLocked(next_time);
      }
      if (is_shutting_down_)
        break;

      if (item.needs_reset_) {
        require_reset_ = true;
        if (item.frame_data_)
          delete[] item.frame_data_;
        continue;
      }
    }

    if (item.frame_data_) {
      if (SendFrame(item.frame_data_)) {
        Autolock l(lock_);
        frame_delays_.push_back(GetCurrentMillis() - item.time_);
        // fprintf(stderr, "Sent frame for %ld at %ld\n", item.time_, GetCurrentMillis());
      } else {
        fprintf(stderr, "Scheduling reset after failed frame\n");
        require_reset_ = true;
      }
      delete[] item.frame_data_;
    }
  }

  CloseSocket();
}

bool TclRenderer::PopNextWorkItemLocked(
    TclRenderer::WorkItem* item, int64_t* next_time) {
  if (queue_.empty()) {
    *next_time = 0;
    return false;
  }
  uint64_t cur_time = GetCurrentMillis();
  while (true) {
    *item = queue_.top();
    if (item->needs_reset_) {
      // Ready to reset, and no future item time is known.
      *next_time = 0;
      while (!queue_.empty())
        queue_.pop();
      return true;
    }
    if (item->time_ > cur_time) {
      // No current item, report the time of the future item.
      *next_time = item->time_;
      return false;
    }
    queue_.pop();
    if (queue_.empty()) {
      // Return current item, and report no future time.
      *next_time = 0;
      return true;
    }
    if (queue_.top().time_ > cur_time) {
      // Return current item, and report future item's time.
      *next_time = queue_.top().time_;
      return true;
    }
    // The queue contains another item that is closer to current time.
    // Skip to that item.
  }
}

static void AddTimeMillis(struct timespec* time, uint64_t increment) {
  uint64_t nsec = (increment % 1000000) * 1000000 + time->tv_nsec;
  time->tv_sec += increment / 1000 + nsec / 1000000000;
  time->tv_nsec = nsec % 1000000000;
}

void TclRenderer::WaitForQueueLocked(int64_t next_time) {
  int err;
  if (next_time) {
    struct timespec timeout;
    if (clock_gettime(CLOCK_REALTIME, &timeout) == -1) {
      REPORT_ERRNO("clock_gettime(realtime)");
      CHECK(false);
    }
    AddTimeMillis(&timeout, next_time - GetCurrentMillis());
    err = pthread_cond_timedwait(&cond_, &lock_, &timeout);
  } else {
    err = pthread_cond_wait(&cond_, &lock_);
  }
  if (err != 0 && err != ETIMEDOUT) {
    fprintf(stderr, "Unable to wait on condition: %d\n", err);
    CHECK(false);
  }
}

void TclRenderer::CloseSocket() {
  if (socket_ != -1) {
    close(socket_);
    socket_ = -1;
  }
}

bool TclRenderer::Connect() {
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
  snprintf(addr_str, sizeof(addr_str),
           "192.168.60.%d", (49 + controller_id_));

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

bool TclRenderer::SendPacket(const void* data, int size) {
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

bool TclRenderer::InitController() {
  static const uint8_t MSG_INIT[] = {0xC5, 0x77, 0x88, 0x00, 0x00};
  static const double MSG_INIT_DELAY = 0.1;
  static const uint8_t MSG_RESET[] = {0xC2, 0x77, 0x88, 0x00, 0x00};
  static const double MSG_RESET_DELAY = 5;

  if (!Connect())
    return false;
  if (init_sent_ && !require_reset_)
    return true;

  if (require_reset_) {
    if (init_sent_)
      fprintf(stderr, "Performing a requested reset\n");
    if (!SendPacket(MSG_RESET, sizeof(MSG_RESET)))
      return false;
    require_reset_ = false;
    Sleep(MSG_RESET_DELAY);
  }

  if (!SendPacket(MSG_INIT, sizeof(MSG_INIT)))
    return false;
  Sleep(MSG_INIT_DELAY);

  init_sent_ = true;
  SetLastReplyTime();
  return true;
}

bool TclRenderer::SendFrame(uint8_t* frame_data) {
  static const uint8_t MSG_START_FRAME[] = {0xC5, 0x77, 0x88, 0x00, 0x00};
  static const uint8_t MSG_END_FRAME[] = {0xAA, 0x01, 0x8C, 0x01, 0x55};
  static const uint8_t FRAME_MSG_PREFIX[] = {
      0x88, 0x00, 0x68, 0x3F, 0x2B, 0xFD,
      0x60, 0x8B, 0x95, 0xEF, 0x04, 0x69};
  static const uint8_t FRAME_MSG_SUFFIX[] = {0x00, 0x00, 0x00, 0x00};

  int mgs_start_delay_us;
  int mgs_data_delay_us;
  {
    Autolock l(lock_);
    mgs_start_delay_us = mgs_start_delay_us_;
    mgs_data_delay_us = mgs_data_delay_us_;
  }

  ConsumeReplyData();
  if (!SendPacket(MSG_START_FRAME, sizeof(MSG_START_FRAME)))
    return false;
  Sleep(((double) mgs_start_delay_us) / 1000000.0);

  uint8_t packet[sizeof(FRAME_MSG_PREFIX) + 1024 + sizeof(FRAME_MSG_SUFFIX)];
  memcpy(packet, FRAME_MSG_PREFIX, sizeof(FRAME_MSG_PREFIX));
  memcpy(packet + sizeof(FRAME_MSG_PREFIX) + 1024,
         FRAME_MSG_SUFFIX, sizeof(FRAME_MSG_SUFFIX));

  int message_idx = 0;
  int frame_data_pos = 0;
  while (frame_data_pos < FRAME_DATA_LEN) {
    packet[1] = message_idx++;
    memcpy(packet + sizeof(FRAME_MSG_PREFIX),
           frame_data + frame_data_pos, 1024);
    frame_data_pos += 1024;

    if (!SendPacket(packet, sizeof(packet)))
      return false;
    Sleep(((double) mgs_data_delay_us) / 1000000.0);
  }
  CHECK(frame_data_pos == FRAME_DATA_LEN);
  CHECK(message_idx == 12);

  if (!SendPacket(MSG_END_FRAME, sizeof(MSG_END_FRAME)))
    return false;
  ConsumeReplyData();
  frames_sent_after_reply_++;
  return true;
}

void TclRenderer::ConsumeReplyData() {
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

void TclRenderer::SetLastReplyTime() {
  last_reply_time_ = GetCurrentMillis();
  frames_sent_after_reply_ = 0;
}

