// Controls TCL controller.

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <deque>

#define uint8_t  unsigned char

#define STRAND_COUNT    8
#define STRAND_LENGTH   512

struct Layout {
  Layout() {
    memset(lengths, 0, sizeof(lengths));
  }

  int x[STRAND_COUNT][STRAND_LENGTH];
  int y[STRAND_COUNT][STRAND_LENGTH];
  int lengths[STRAND_COUNT];
};

// To use this class:
//   renderer = TclRenderer(controller_id)
//   renderer.set_layout('layout.dxf', width, height)
//   // 'image_colors' has tuples with 3 RGB bytes for each pixel,
//   //  with 'height' number of sequential rows, each row having
//   // length of 'width' pixels.
//   renderer.send_frame(image_color)
class TclRenderer {
 public:
  TclRenderer(
      int controller_id, int width, int height, double gamma);

  void Shutdown();

  void SetLayout(const Layout& layout);

  void SetGamma(double gamma);
  void SetGammaRanges(
      int r_min, int r_max, double r_gamma,
      int g_min, int g_max, double g_gamma,
      int b_min, int b_max, double b_gamma);
  void ApplyGamma(Strands* strands);

  void ScheduleImage(uint8_t* image_data, uint64_t time);

 private:
  TclRenderer();
  TclRenderer(const TclRenderer& src);
  ~TclRenderer();
  TclRenderer& operator=(const TclRenderer& rhs);

  struct Strands {
    Strands() {
      for (int i = 0; i < STRAND_COUNT; i++) {
        colors[i] = new uint8_t[STRAND_LENGTH * 3];
        lengths[i] = 0;
      }
    }

    ~Strands() {
      for (int i = 0; i < STRAND_COUNT; i++) {
        delete[] colors[i];
      }
    }

    uint8_t* colors[STRAND_COUNT];
    int lengths[STRAND_COUNT];

  private:
    Strands(const Strands& src);
    Strands& operator=(const Strands& rhs);
  };

  struct WorkItem {
    WorkItem(bool needs_reset, uint8_t* frame_data, uint64_t time)
        : needs_reset_(needs_reset), frame_data_(frame_data), time_(time) {}

    bool needs_reset_;
    uint8_t* frame_data_;
    uint64_t* time_;
  };

  void Run();
  static void* ThreadEntry(void* arg);

  // Socket communication funtions are invoked from worker thread only.
  bool Connect();
  bool InitController();
  void SendFrame(uint8_t* frame_data);
  bool SendPacket(const void* data, int size);
  void ConsumeReplyData();

  Strands* ConvertImageToStrands(uint8_t* image_data);
  void ScheduleStrands(Strands* strands, uint64_t time);

  static uint8_t* ConvertStrandsToFrame(Strands* strands);
  static int BuildFrameColorSeq(
    Strands* strands, int led_id, int color_component, uint8_t* dst);

  int controller_id_;
  int width_;
  int height_;
  bool init_sent_;
  bool is_shutting_down_;
  StrandLayout layout_[STRAND_COUNT];
  double gamma_g_[256];
  double gamma_b_[256];
  Layout layout_;
  int socket_;
  bool require_reset_;
  std::deque<WorkItem> queue_;
  pthread_mutex_t lock_;
  pthread_cond_t cond_;
};


#define FRAME_DATA_LEN  (STRAND_LENGTH * 8 * 3)


#define REPORT_ERRNO(name)                           \
  fprintf(stderr, "Failure in '%s' call: %d, %s",    \
          name, errno, strerror(errno));

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
  Autolock(const TclRenderer& src);
  Autolock& operator=(const Autolock& rhs);

  pthread_mutex_t* lock_;
};

TclRenderer::TclRenderer(
    int controller_id, int width, int height, double gamma)
    : controller_id_(controller_id), width_(width), height_(height),
      init_sent_(false), is_shutting_down_(false), socket_(-1),
      require_reset_(true), lock_(PTHREAD_MUTEX_INITIALIZER),
      cond_(PTHREAD_COND_INITIALIZER) {
  memset(layout, 0, sizeof(layout));
  SetGamma(gamma);
}

TclRenderer::~TclRenderer() {
  for (std::deque<WorkItem>::iterator it = queue_.begin();
       it != queue.end(); it++) {
    if ((*it).frame_data_)
      delete[] (*it).frame_data_;
  }
  pthread_cond_destroy(&cond_);
  pthread_mutex_destroy(&lock_);
}

void TclRenderer::Shutdown() {
  {
    Autolock(lock_);
    is_shutting_down_ = true;
    pthread_cond_broadcast(&cond_);
  }

  // TODO(igorc): Join the thread.
}

void TclRenderer::SetLayout(const Layout& layout) {
  Autolock(lock_);
  layout_ = layout;
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

  Autolock(lock_);
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

void TclRenderer::ScheduleImage(uint8_t* image_data, uint64_t time) {
  Strands* strands = ConvertImageToStrands(image_data);
  ApplyGamma(strands);
  ScheduleStrands(strands, time);
  delete strands;
}

void TclRenderer::ScheduleStrands(Strands* strands, uint64_t time) {
  uint8_t* frame_data = ConvertStrandsToFrame(strands);

  Autolock(lock_);
  if (is_shutting_down_) {
    delete[] frame_data;
    return;
  }
  // TODO(igorc): Recalculate time from relative to absolute.
  uint64_t abs_time = time;
  queue_.push_back(WorkItem(false, frame_data, abs_time));
  delete strands;
  int err = pthread_cond_broadcast(&cond_);
  if (err != 0) {
    fprintf(stderr, "Unable to signal condition: %d\n", err);
    CHECK(false);
  }
}

Strands* TclRenderer::ConvertImageToStrands(uint8_t* image_data) {
  Autolock(lock_);
  Strands* strands = new Strands();
  for (int strand_id  = 0; strand_id < STRAND_COUNT; strand_id++) {
    int* x = layout_.x[strand_id];
    int* y = layout_.y[strand_id];
    int len = std::min(layout_.lengths[strand_id], STRAND_LENGTH);
    uint8_t* dst_colors = strands->colors[strand_id];
    for (int i = 0; i < len; i++) {
      int color_idx = y[i] * width_ + x[i];
      int dst_idx = i * 3;
      dst_colors[dst_idx] = image_data[color_idx];
      dst_colors[dst_idx + 1] = image_data[color_idx + 1];
      dst_colors[dst_idx + 2] = image_data[color_idx + 2];
    }
    strands->lengths[strand_id] = len;
  }
  return strands;
}

void TclRenderer::ApplyGamma(Strands* strands) {
  Autolock(lock_);
  for (int strand_id  = 0; strand_id < STRAND_COUNT; strand_id++) {
    int len = strands->lengths[strand_id] * 3;
    int8_t* colors = strands->colors[strand_id];
    for (int i = 0; i < len; i += 3) {
      colors[i] = gamma_r_[colors[i]];
      colors[i + 1] = gamma_g_[colors[i + 1]];
      colors[i + 2] = gamma_b_[colors[i + 2]];
    }
  }
}

// static
uint8_t* TclRenderer::ConvertStrandsToFrame(Strands* strands) {
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
    Strands* strands, int led_id, int color_component, uint8_t* dst) {
  int pos = 0;
  int color_bit_mask = 0x80;
  while (color_bit_mask > 0) {
    uint8_t dst_byte = 0;
    for (int strand_id  = 0; strand_id < STRAND_COUNT; strand_id++) {
      if (led_id >= strands.lengths[strand_id])
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
  req.tv_nsec = (long) ((seconds - req.tv_sec) * 1000000.0);
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
}

void TclRenderer::Run() {
  while (true) {
    {
      Autolock(lock_);
      if (is_shutting_down_)
        break;
    }

    if (!InitController()) {
      Sleep(1);
      continue;
    }

    WorkItem item;
    {
      Autolock(lock_);
      while (!is_shutting_down_ && items_.empty()) {
        int err = pthread_cond_wait(&cond_, &lock_);
        if (err != 0) {
          fprintf(stderr, "Unable to wait on condition: %d\n", err);
          CHECK(false);
        }
      }
      if (is_shutting_down_)
        break;

      item = queue_.front();
      // TODO(igorc): Delay processing until requested time.
      queue_.pop_front();

      if (item.needs_reset_) {
        require_reset_ = true;
        if (item.frame_data_) {
          delete[] item.frame_data_;
        continue;
      }
    }

    if (item.frame_data_) {
      if (!SendFrame(item.frame_data_)) {
        fprintf(stderr, "Scheduling reset after failed frame\n");
        require_reset_ = true;
      }
      delete[] item.frame_data_;
    }
  }

  CloseSocket();
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

  socket_ = socket.socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
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
  si_local.sin_port = htons(PORT);
  si_local.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(socket_, &si_local, sizeof(si_local)) == -1) {
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
          socket_, &si_remote, sizeof(si_remote))) == -1) {
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

  // TODO(igorc): Re-init if there's still no reply data in several seconds.
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
  return true;
}

bool TclRenderer::SendFrame(uint8_t* frame_data) {
  static const uint8_t MSG_START_FRAME[] = {0xC5, 0x77, 0x88, 0x00, 0x00};
  static const double MSG_START_FRAME_DELAY = 0.0005;
  static const uint8_t MSG_END_FRAME[] = {0xAA, 0x01, 0x8C, 0x01, 0x55};
  static const uint8_t MSG_REPLY[] = {0x55, 0x00, 0x00, 0x00, 0x00};
  static const uint8_t FRAME_MSG_PREFIX[] = {
      0x88, 0x00, 0x68, 0x3F, 0x2B, 0xFD,
      0x60, 0x8B, 0x95, 0xEF, 0x04, 0x69};
  static const uint8_t FRAME_MSG_SUFFIX[] = {0x00, 0x00, 0x00, 0x00};
  static const double FRAME_MSG_DELAY = 0.0015;
  static const int FRAME_DATA_SIZE_IM_MSG = 1024;

  ConsumeReplyData();
  if (!SendPacket(MSG_START_FRAME, sizeof(MSG_START_FRAME)))
    return false;
  Sleep(MSG_START_FRAME_DELAY);

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
    Sleep(FRAME_MSG_DELAY);
  }
  CHECK(frame_data_pos == FRAME_DATA_LEN);
  CHECK(message_idx == 12);

  if (!SendPacket(MSG_END_FRAME, sizeof(MSG_END_FRAME)))
    return false;
  ConsumeReplyData();
}

void TclRenderer::ConsumeReplyData() {
  uint8_t buf[65536];
  while (true) {
    ssize_t size = TEMP_FAILURE_RETRY(
        recv(socket_, buf, sizeof(buf), MSG_DONTWAIT));
    if (size != -1)
      continue;
    if (errno != EAGAIN && errno != EWOULDBLOCK)
      REPORT_ERRNO("recv");
    break;
  }
}

