// Copyright 2015, Igor Chernyshev.
// Licensed under The MIT License
//

#include "kinect.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "utils.h"
#include "../libfreenect/include/libfreenect.h"

#define CHECK_FREENECT(a)  CHECK(!(a))

#define DEVICE_WIDTH     640
#define DEVICE_HEIGHT    480
#define DEVICE_ROW_SIZE  (DEVICE_WIDTH * 3)

///////////////////////////////////////////////////////////////////////////////
// KinectDevice
///////////////////////////////////////////////////////////////////////////////

// Represents connection to a single device.
// All methods are invoked under devices_mutex_.
class KinectDevice {
 public:
  KinectDevice(freenect_device* device);
  ~KinectDevice();

  void Setup();
  void Teardown();

  void HandleDepthData(void* depth_data);
  void HandleCameraData(void* rgb_data);

  const freenect_device* device() { return device_; }
  const uint8_t* last_camera_data() const { return last_camera_data_; }
  const uint8_t* last_depth_data() const { return last_depth_data_; }

  bool CheckAndClearCameraUpdate();
  bool CheckAndClearDepthUpdate();

 private:
  KinectDevice(const KinectDevice& src);
  KinectDevice& operator=(const KinectDevice& rhs);

  freenect_device* device_;
  uint8_t* camera_data1_;
  uint8_t* camera_data2_;
  uint8_t* depth_data1_;
  uint8_t* depth_data2_;
  uint8_t* last_camera_data_ = nullptr;
  uint8_t* last_depth_data_ = nullptr;
  bool has_camera_update_ = false;
  bool has_depth_update_ = false;
};

KinectDevice::KinectDevice(freenect_device* device) : device_(device) {
  CHECK(device_);
  int data_size = DEVICE_HEIGHT * DEVICE_ROW_SIZE;
  camera_data1_ = new uint8_t[data_size];
  camera_data2_ = new uint8_t[data_size];
  depth_data1_ = new uint8_t[data_size];
  depth_data2_ = new uint8_t[data_size];
}

KinectDevice::~KinectDevice() {
  if (device_)
    freenect_close_device(device_);

  delete[] camera_data1_;
  delete[] camera_data2_;
  delete[] depth_data1_;
  delete[] depth_data2_;
}

void KinectDevice::Setup() {
  CHECK_FREENECT(freenect_set_led(device_, LED_RED));
  // http://openkinect.org/wiki/Protocol_Documentation#RGB_Camera
  // !!! 0: uncompressed 16 bit depth stream between 0x0000 and 0x07ff
  // (2^11-1). Causes bandwidth issues; will drop packets.
  CHECK_FREENECT(freenect_set_depth_mode(
      device_, freenect_find_depth_mode(
	  FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT)));
  CHECK_FREENECT(freenect_set_video_mode(
      device_, freenect_find_video_mode(
	  FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB)));
  CHECK_FREENECT(freenect_set_depth_buffer(device_, depth_data1_));
  CHECK_FREENECT(freenect_set_video_buffer(device_, camera_data1_));

  CHECK_FREENECT(freenect_start_depth(device_));
  CHECK_FREENECT(freenect_start_video(device_));
}

void KinectDevice::Teardown() {
  freenect_stop_depth(device_);
  freenect_stop_video(device_);
}

bool KinectDevice::CheckAndClearDepthUpdate() {
  bool result = has_depth_update_;
  has_depth_update_ = false;
  return result;
}

bool KinectDevice::CheckAndClearCameraUpdate() {
  bool result = has_camera_update_;
  has_camera_update_ = false;
  return result;
}

void KinectDevice::HandleDepthData(void* depth_data) {
  last_depth_data_ = reinterpret_cast<uint8_t*>(depth_data);
  CHECK_FREENECT(freenect_set_depth_buffer(
      device_,
      last_depth_data_ == depth_data1_ ? depth_data2_ : depth_data2_));
  has_depth_update_ = true;
}

void KinectDevice::HandleCameraData(void* rgb_data) {
  last_camera_data_ = reinterpret_cast<uint8_t*>(rgb_data);
  CHECK_FREENECT(freenect_set_video_buffer(
      device_,
      last_camera_data_ == camera_data1_ ? camera_data2_ : camera_data2_));
  has_camera_update_ = true;
}

///////////////////////////////////////////////////////////////////////////////
// KinectRange
///////////////////////////////////////////////////////////////////////////////

class KinectRangeImpl : public KinectRange {
 public:
  KinectRangeImpl();
  ~KinectRangeImpl() override;

  void Start(int fps) override;

  int GetWidth() const override;
  int GetHeight() const override;
  void GetDepthData(uint8_t* dst) const override;
  void GetCameraData(uint8_t* dst) const override;

  Bytes* GetAndClearLastDepthImage() override;
  Bytes* GetAndClearLastCameraImage() override;

 private:
  static KinectRangeImpl* GetInstanceImpl();

  static void* RunFreenectLoop(void* arg);
  static void OnDepthCallback(
      freenect_device* dev, void* depth_data, uint32_t timestamp);
  static void OnCameraCallback(
      freenect_device* dev, void* rgb_data, uint32_t timestamp);

  void RunFreenectLoop();
  void HandleDepthData(freenect_device* dev, void* depth_data);
  void HandleCameraData(freenect_device* dev, void* rgb_data);
  KinectDevice* FindDeviceLocked(freenect_device* dev) const;

  static void* RunMergerLoop(void* arg);
  void RunMergerLoop();
  void MergeImages();
  bool CopyDeviceDataLocked(
      uint8_t* dst, const uint8_t* src, int device_idx);

  int fps_;
  freenect_context* context_ = nullptr;
  pthread_t freenect_thread_;
  pthread_t merger_thread_;
  volatile bool should_exit_ = false;
  mutable pthread_mutex_t devices_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  mutable pthread_mutex_t merger_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  std::vector<KinectDevice*> devices_;
  bool has_started_thread_ = false;
  int data_size_ = 0;
  uint8_t* camera_data_ = nullptr;
  uint8_t* depth_data_ = nullptr;
  bool has_new_depth_image_ = false;
  bool has_new_camera_image_ = false;
};

KinectRange* KinectRange::instance_ = new KinectRangeImpl();

// static
KinectRange* KinectRange::GetInstance() {
  return instance_;
}

// static
KinectRangeImpl* KinectRangeImpl::GetInstanceImpl() {
  return reinterpret_cast<KinectRangeImpl*>(GetInstance());
}

KinectRangeImpl::KinectRangeImpl() : fps_(15) {}

KinectRangeImpl::~KinectRangeImpl() {
  should_exit_ = true;
  pthread_join(freenect_thread_, NULL);
  pthread_join(merger_thread_, NULL);

  for (auto device : devices_) {
    device->Teardown();
    delete device;
  }

  if (context_)
    freenect_shutdown(context_);

  delete[] camera_data_;
  delete[] depth_data_;
}

void KinectRangeImpl::Start(int fps) {
  Autolock l1(merger_mutex_);
  Autolock l2(devices_mutex_);
  if (has_started_thread_)
    return;
  has_started_thread_ = true;

  fps_ = fps;

  CHECK_FREENECT(freenect_init(&context_, NULL));
  freenect_set_log_level(context_, FREENECT_LOG_DEBUG);
  freenect_select_subdevices(
      context_, (freenect_device_flags) FREENECT_DEVICE_CAMERA);

  int device_count = freenect_num_devices(context_);
  fprintf(stderr, "Found %d Kinect devices\n", device_count);

  for (int i = 0; i < device_count; ++i) {
    freenect_device* device = nullptr;
    CHECK_FREENECT(freenect_open_device(context_, &device, i));
    freenect_set_depth_callback(device, OnDepthCallback);
    freenect_set_video_callback(device, OnCameraCallback);
    KinectDevice* kinectDevice = new KinectDevice(device);
    kinectDevice->Setup();
    devices_.push_back(kinectDevice);
  }

  data_size_ = DEVICE_HEIGHT * DEVICE_ROW_SIZE * device_count;
  camera_data_ = new uint8_t[data_size_];
  depth_data_ = new uint8_t[data_size_];
  memset(camera_data_, 0, data_size_);
  memset(depth_data_, 0, data_size_);

  CHECK(!pthread_create(&merger_thread_, NULL, RunMergerLoop, this));
  CHECK(!pthread_create(&freenect_thread_, NULL, RunFreenectLoop, this));
}

// static
void* KinectRangeImpl::RunFreenectLoop(void* arg) {
  reinterpret_cast<KinectRangeImpl*>(arg)->RunFreenectLoop();
  return NULL;
}

void KinectRangeImpl::RunFreenectLoop() {
  while (!should_exit_ && freenect_process_events(context_) >= 0) {
    // Let callbacks do the work.
    fprintf(stderr, "RunFreenectLoop\n");
  }
}

// static
void KinectRangeImpl::OnDepthCallback(
    freenect_device* dev, void* depth_data, uint32_t timestamp) {
  (void) timestamp;
  GetInstanceImpl()->HandleDepthData(dev, depth_data);
}

// static
void KinectRangeImpl::OnCameraCallback(
    freenect_device* dev, void* rgb_data, uint32_t timestamp) {
  (void) timestamp;
  GetInstanceImpl()->HandleCameraData(dev, rgb_data);
}

KinectDevice* KinectRangeImpl::FindDeviceLocked(freenect_device* dev) const {
  for (auto device : devices_) {
    if (device->device() == dev) {
      return device;
    }
  }
  return nullptr;
}

void KinectRangeImpl::HandleDepthData(
    freenect_device* dev, void* depth_data) {
  Autolock l(devices_mutex_);
  KinectDevice* device = FindDeviceLocked(dev);
  CHECK(device);
  device->HandleDepthData(depth_data);
  fprintf(stderr, "HandleDepthData\n");
}

void KinectRangeImpl::HandleCameraData(
    freenect_device* dev, void* rgb_data) {
  Autolock l(devices_mutex_);
  KinectDevice* device = FindDeviceLocked(dev);
  CHECK(device);
  device->HandleCameraData(rgb_data);
  fprintf(stderr, "HandleCameraData\n");
}

// static
void* KinectRangeImpl::RunMergerLoop(void* arg) {
  reinterpret_cast<KinectRangeImpl*>(arg)->RunMergerLoop();
  return NULL;
}

void KinectRangeImpl::RunMergerLoop() {
  uint32_t ms_per_frame = (uint32_t) (1000.0 / (double) fps_);
  uint64_t next_render_time = GetCurrentMillis() + ms_per_frame;
  while (!should_exit_) {
    uint32_t remaining_time = 0;
    uint64_t now = GetCurrentMillis();
    if (next_render_time > now)
      remaining_time = next_render_time - now;
    if (remaining_time > 0)
      Sleep(((double) remaining_time) / 1000.0);
    next_render_time += ms_per_frame;

    MergeImages();
  }
}

void KinectRangeImpl::MergeImages() {
  Autolock l1(merger_mutex_);

  bool has_depth_update = false;
  bool has_camera_update = false;
  {
    // Merge images from all devices into one.
    Autolock l2(devices_mutex_);
    for (size_t i = 0; i < devices_.size(); ++i) {
      KinectDevice* device = devices_[i];
      has_depth_update |= device->CheckAndClearDepthUpdate();
      has_camera_update |= device->CheckAndClearCameraUpdate();
      CopyDeviceDataLocked(camera_data_, device->last_camera_data(), i);
      CopyDeviceDataLocked(depth_data_, device->last_depth_data(), i);
      // TODO(igorc): Erase device's part of the image after
      // a few missing updates.
    }
  }

  if (has_depth_update)
    has_new_depth_image_ = true;
  if (has_camera_update)
    has_new_camera_image_ = true;

  /*uint16_t* depth = reinterpret_cast<uint16_t*>(depth_data);
  for (int i=0; i < DEVICE_WIDTH * DEVICE_HEIGHT; ++i) {
    int value = depth[i];
  }*/
}

bool KinectRangeImpl::CopyDeviceDataLocked(
    uint8_t* dst, const uint8_t* src, int device_idx) {
  if (!src) {
    // Keep original zero's in the destination array.
    return false;
  }
  if (devices_.size() == 1) {
    memcpy(dst, src, DEVICE_ROW_SIZE * DEVICE_HEIGHT);
    return true;
  }
  uint8_t* dst_shifted = dst + DEVICE_ROW_SIZE * device_idx;
  int dst_row_size = DEVICE_ROW_SIZE * devices_.size();
  for (int i = 0; i < DEVICE_HEIGHT; ++i) {
    memcpy(dst_shifted + i * dst_row_size,
	   src + i * DEVICE_ROW_SIZE,
	   DEVICE_ROW_SIZE);
  }
  return true;
}

int KinectRangeImpl::GetWidth() const {
  return DEVICE_WIDTH * devices_.size();
}

int KinectRangeImpl::GetHeight() const {
  return DEVICE_HEIGHT;
}

void KinectRangeImpl::GetDepthData(uint8_t* dst) const {
  Autolock l(merger_mutex_);
  memcpy(dst, depth_data_, data_size_);
}

void KinectRangeImpl::GetCameraData(uint8_t* dst) const {
  Autolock l(merger_mutex_);
  memcpy(dst, camera_data_, data_size_);
}

Bytes* KinectRangeImpl::GetAndClearLastDepthImage() {
  Autolock l(merger_mutex_);
  if (!has_new_depth_image_)
    return NULL;
  Bytes* result = new Bytes(depth_data_, data_size_);
  has_new_depth_image_ = false;
  return result;
}

Bytes* KinectRangeImpl::GetAndClearLastCameraImage() {
  Autolock l(merger_mutex_);
  if (!has_new_camera_image_)
    return NULL;
  // Unpack 3-byte RGB into RGBA.
  int width = GetWidth();
  int height = GetHeight();
  int dst_size = width * height * 4;
  uint8_t* dst = new uint8_t[dst_size];
  for (int y = 0; y < height; ++y) {
    uint8_t* dst_row = dst + y * width * 4;
    const uint8_t* src_row = camera_data_ + y * width * 3;
    for (int x = 0; x < width; ++x) {
      memcpy(dst_row + x * 4, src_row + x * 3, 3);
      dst_row[3] = 0;
    }
  }
  Bytes* result = new Bytes(dst, dst_size);
  has_new_camera_image_ = false;
  return result;
}
