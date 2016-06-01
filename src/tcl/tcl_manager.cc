// Copyright 2014, Igor Chernyshev.

#include "tcl/tcl_manager.h"

#include <string.h>

#include "tcl/tcl_controller.h"
#include "util/lock.h"
#include "util/logging.h"
#include "util/time.h"

namespace {


}  // namespace

TclManager::TclManager()
    : lock_(PTHREAD_MUTEX_INITIALIZER),
      cond_(PTHREAD_COND_INITIALIZER) {
  base_time_ = GetCurrentMillis();
}

TclManager::~TclManager() {
  if (has_started_thread_) {
    {
      Autolock l(lock_);
      is_shutting_down_ = true;
      pthread_cond_broadcast(&cond_);
    }

    pthread_join(thread_, nullptr);
  }

  ResetImageQueue();

  for (std::vector<TclController*>::iterator it = controllers_.begin();
        it != controllers_.end(); ++it) {
    delete (*it);
  }

  pthread_cond_destroy(&cond_);
  pthread_mutex_destroy(&lock_);
}

void TclManager::AddController(
    int id, int width, int height,
    const LedLayout& layout, double gamma) {
  Autolock l(lock_);
  CHECK(!controllers_locked_);
  CHECK(FindControllerLocked(id) == nullptr);
  TclController* controller = new TclController(
      id, width, height, layout, gamma);
  controllers_.push_back(controller);
}

TclController* TclManager::FindControllerLocked(int id) {
  for (std::vector<TclController*>::iterator it = controllers_.begin();
        it != controllers_.end(); ++it) {
    if ((*it)->id() == id)
      return *it;
  }
  return nullptr;
}

void TclManager::LockControllers() {
  Autolock l(lock_);
  controllers_locked_ = true;
}

bool TclManager::GetControllerImageSize(int controller_id, int* w, int* h) {
  Autolock l(lock_);
  TclController* controller = FindControllerLocked(controller_id);
  if (!controller)
    return false;
  *w = controller->width();
  *h = controller->height();
  return true;
}

void TclManager::StartMessageLoop(int fps, bool enable_net) {
  Autolock l(lock_);
  CHECK(controllers_locked_);
  if (has_started_thread_)
    return;
  fps_ = fps;
  enable_net_ = enable_net;
  has_started_thread_ = true;
  int err = pthread_create(&thread_, nullptr, &ThreadEntry, this);
  if (err != 0) {
    fprintf(stderr, "pthread_create failed with %d\n", err);
    CHECK(false);
  }
}

void TclManager::SetGammaRanges(
    int r_min, int r_max, double r_gamma,
    int g_min, int g_max, double g_gamma,
    int b_min, int b_max, double b_gamma) {
  Autolock l(lock_);
  CHECK(controllers_locked_);
  for (std::vector<TclController*>::iterator it = controllers_.begin();
        it != controllers_.end(); ++it) {
    (*it)->SetGammaRanges(
        r_min, r_max, r_gamma, g_min, g_max, g_gamma, b_min, b_max, b_gamma);
  }
}

void TclManager::SetHdrMode(HdrMode mode) {
  Autolock l(lock_);
  CHECK(controllers_locked_);
  for (std::vector<TclController*>::iterator it = controllers_.begin();
        it != controllers_.end(); ++it) {
    (*it)->SetHdrMode(mode);
  }
}

void TclManager::SetAutoResetAfterNoDataMs(int value) {
  Autolock l(lock_);
  auto_reset_after_no_data_ms_ = value;
}

std::string TclManager::GetInitStatus() {
  Autolock l(lock_);
  std::ostringstream result;
  for (std::vector<TclController*>::iterator it = controllers_.begin();
        it != controllers_.end(); ++it) {
    if (it != controllers_.begin())
      result << ", ";
    result << "TCL" << (*it)->id() << " = ";
    switch ((*it)->init_status()) {
      case INIT_STATUS_UNUSED:
        result << "UNUSED";
        break;
      case INIT_STATUS_OK:
        result << "OK";
        break;
      case INIT_STATUS_FAIL:
        result << "FAIL";
        break;
      case INIT_STATUS_RESETTING:
        result << "RESETTING";
        break;
      default:
        result << "UNKNOWN!?";
        break;
    }
  }
  return result.str();
}

std::vector<int> TclManager::GetAndClearFrameDelays() {
  Autolock l(lock_);
  std::vector<int> result = frame_delays_;
  frame_delays_.clear();
  return result;
}

// static
int TclManager::GetFrameSendDurationMs() {
  return TclController::GetFrameSendDurationMs();
}

std::unique_ptr<RgbaImage> TclManager::GetAndClearLastImage(int controller_id) {
  Autolock l(lock_);
  TclController* controller = FindControllerLocked(controller_id);
  return (controller ? controller->GetAndClearLastImage() : nullptr);
}

std::unique_ptr<RgbaImage> TclManager::GetAndClearLastLedImage(
    int controller_id) {
  Autolock l(lock_);
  TclController* controller = FindControllerLocked(controller_id);
  return (controller ? controller->GetAndClearLastLedImage() : nullptr);
}

int TclManager::GetLastImageId(int controller_id) {
  Autolock l(lock_);
  TclController* controller = FindControllerLocked(controller_id);
  return (controller ? controller->last_image_id() : -1);
}

void TclManager::ScheduleImageAt(
    int controller_id, const RgbaImage& image, int id,
    uint64_t time, bool wakeup) {
  Autolock l(lock_);
  CHECK(has_started_thread_);
  if (is_shutting_down_)
    return;

  TclController* controller = FindControllerLocked(controller_id);
  if (!controller) {
    fprintf(stderr, "Ignoring TclManager::ScheduleImageAt on %d\n", controller_id);
    return;
  }
  if (image.width() != controller->width() ||
      image.height() != controller->height()) {
    fprintf(stderr, "Image/controller size mismatch for %d\n", controller_id);
    return;
  }

  if (time > base_time_) {
    // Align with FPS.
    double frame_num = round(((double) (time - base_time_)) / 1000.0 * fps_);
    time = base_time_ + (uint64_t) (frame_num * 1000.0 / fps_);
    // fprintf(stderr, "Changed target time from %ld to %ld, f=%.2f\n",
    //         time, time, frame_num);
  }

  queue_.push(WorkItem(false, controller, image, id, time));

  // fprintf(stderr, "Scheduled item with time=%ld\n", time_abs);

  if (wakeup)
    WakeupLocked();
}

void TclManager::Wakeup() {
  Autolock l(lock_);
  WakeupLocked();
}

void TclManager::WakeupLocked() {
  int err = pthread_cond_broadcast(&cond_);
  if (err != 0) {
    fprintf(stderr, "Unable to signal condition: %d\n", err);
    CHECK(false);
  }
}

void TclManager::SetEffectImage(int controller_id, const RgbaImage& image) {
  Autolock l(lock_);
  TclController* controller = FindControllerLocked(controller_id);
  if (controller)
    controller->SetEffectImage(image);
}

void TclManager::EnableRainbow(int controller_id, int x) {
  Autolock l(lock_);
  TclController* controller = FindControllerLocked(controller_id);
  if (controller)
    controller->EnableRainbow(x);
}

void TclManager::ResetImageQueue() {
  Autolock l(lock_);
  while (!queue_.empty()) {
    WorkItem item = queue_.top();
    queue_.pop();
  }
}

std::vector<int> TclManager::GetFrameDataForTest(
    int controller_id, const RgbaImage& image) {
  Autolock l(lock_);
  std::vector<int> result;
  TclController* controller = FindControllerLocked(controller_id);
  if (controller) {
    std::vector<uint8_t> frame_data = controller->GetFrameDataForTest(image);
    for (auto b : frame_data) {
      result.push_back(b);
    }
  }
  return result;
}

int TclManager::GetQueueSize() {
  Autolock l(lock_);
  return queue_.size();
}

// static
void* TclManager::ThreadEntry(void* arg) {
  TclManager* self = reinterpret_cast<TclManager*>(arg);
  self->Run();
  return nullptr;
}

struct FoundItem {
  FoundItem(TclController* controller) : controller(controller) {}

  FoundItem(const FoundItem& src) {
    *this = src;
  }

  FoundItem& operator=(const FoundItem& rhs) {
    controller = rhs.controller;
    frame_data_ = rhs.frame_data_;
    return *this;
  }

  TclController* controller;
  std::vector<uint8_t> frame_data_;
};

void TclManager::Run() {
  while (true) {
    {
      Autolock l(lock_);
      if (is_shutting_down_)
        break;

      if (enable_net_) {
        for (std::vector<TclController*>::iterator it = controllers_.begin();
              it != controllers_.end(); ++it) {
          (*it)->UpdateAutoReset(auto_reset_after_no_data_ms_);
        }
      } else {
        for (std::vector<TclController*>::iterator it = controllers_.begin();
              it != controllers_.end(); ++it) {
          (*it)->MarkInitialized();
        }
      }
    }

    if (enable_net_) {
      bool failed_init = false;
      for (std::vector<TclController*>::iterator it = controllers_.begin();
            it != controllers_.end(); ++it) {
        InitStatus status = (*it)->InitController();
        failed_init |= (status == INIT_STATUS_FAIL);
      }
      if (failed_init) {
        Sleep(1);
        continue;
      }
    }

    std::vector<FoundItem> items;
    uint64_t min_time = GetCurrentMillis();
    {
      Autolock l(lock_);
      while (!is_shutting_down_) {
        int64_t next_time;
        WorkItem item(false, nullptr, RgbaImage(), 0, 0);
        if (!PopNextWorkItemLocked(&item, &next_time)) {
          if (!items.empty())
            break;
          WaitForQueueLocked(next_time);
          continue;
        }

        //fprintf(stderr, "Found item with time=%ld\n", item.time_);

        if (item.needs_reset) {
          item.controller->ScheduleReset();
          continue;
        }

        if (item.img.empty()) {
          fprintf(stderr, "Skipping an item with no image on %d\n",
                  item.controller->id());
          continue;
        }

        FoundItem found_item(item.controller);
        InitStatus status = INIT_STATUS_FAIL;
        item.controller->BuildFrameDataForImage(
            &found_item.frame_data_, &item.img, item.id, &status);
        if (found_item.frame_data_.empty()) {
          if (status == INIT_STATUS_FAIL) {
            fprintf(stderr, "Failed to build frame_data for an image on %d\n",
                    item.controller->id());
          }
          continue;
        }

        if (item.time < min_time)
          min_time = item.time;
        items.push_back(found_item);
      }

      if (is_shutting_down_)
        break;
    }

    //fprintf(stderr, "Processing %ld items\n", items.size());

    if (!enable_net_) {
      Autolock l(lock_);
      frame_delays_.push_back(GetCurrentMillis() - min_time);
      continue;
    }

    for (std::vector<FoundItem>::iterator it = items.begin();
          it != items.end(); ++it) {
      if (it->controller->SendFrame(it->frame_data_.data())) {
        Autolock l(lock_);
        frame_delays_.push_back(GetCurrentMillis() - min_time);
        // fprintf(stderr, "Sent frame for %ld at %ld\n", item.time_, GetCurrentMillis());
      } else {
        fprintf(stderr, "Scheduling reset after failed frame\n");
        it->controller->ScheduleReset();
      }
    }
  }
}

bool TclManager::PopNextWorkItemLocked(
    TclManager::WorkItem* item, int64_t* next_time) {
  if (queue_.empty()) {
    *next_time = 0;
    return false;
  }
  uint64_t cur_time = GetCurrentMillis();
  while (true) {
    *item = queue_.top();
    if (item->needs_reset) {
      // Ready to reset, and no future item time is known.
      *next_time = 0;
      while (!queue_.empty())
        queue_.pop();
      return true;
    }
    if (item->time > cur_time) {
      // No current item, report the time of the future item.
      *next_time = item->time;
      return false;
    }
    queue_.pop();
    if (queue_.empty()) {
      // Return current item, and report no future time.
      *next_time = 0;
      return true;
    }
    if (queue_.top().controller != item->controller ||
        queue_.top().time > cur_time) {
      // Return current item, and report future item's time.
      *next_time = queue_.top().time;
      return true;
    }
    // The queue contains another item that is closer to current time.
    // Skip to that item.
  }
}

void TclManager::WaitForQueueLocked(int64_t next_time) {
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
