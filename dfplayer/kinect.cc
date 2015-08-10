// Copyright 2015, Igor Chernyshev.
// Licensed under The MIT License
//

#include "kinect.h"

#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "utils.h"
#include "../external/kkonnect/external/libfreenect/include/libfreenect.h"

// TODO(igorc): Add atexit() to stop this, tcl and visualizer threads,
// and to unblock all waiting Python threads.

#define CHECK_FREENECT(call)                                            \
    { int res__ = (call);                                               \
      if (res__) {                                                      \
        fprintf(stderr, "EXITING with check-fail at %s (%s:%d): %d"     \
                ". Condition = '" TOSTRING(call) "'\n",                 \
                __FILE__, __FUNCTION__, __LINE__, res__);               \
        exit(-1);                                                       \
      } }


#define DEVICE_WIDTH     640
#define DEVICE_HEIGHT    480
#define DEVICE_PIXELS    (DEVICE_WIDTH * DEVICE_HEIGHT)

#define MAX_DEVICE_OPEN_ATTEMPTS   10

///////////////////////////////////////////////////////////////////////////////
// KinectDevice
///////////////////////////////////////////////////////////////////////////////

// Represents connection to a single device.
// All methods are invoked under devices_mutex_.
class KinectDevice {
 public:
  KinectDevice(freenect_device* device);
  ~KinectDevice();

  void Setup(bool camera_enabled, bool depth_enabled);
  void Teardown();

  void HandleDepthData(void* depth_data);
  void HandleCameraData(void* rgb_data);

  const freenect_device* device() { return device_; }
  const uint8_t* last_camera_data() const { return last_camera_data_; }
  const uint16_t* last_depth_data() const { return last_depth_data_; }

  bool CheckAndClearCameraUpdate();
  bool CheckAndClearDepthUpdate();

 private:
  KinectDevice(const KinectDevice& src);
  KinectDevice& operator=(const KinectDevice& rhs);

  freenect_device* device_;
  uint8_t* camera_data1_;
  uint8_t* camera_data2_;
  uint16_t* depth_data1_;
  uint16_t* depth_data2_;
  uint8_t* last_camera_data_ = nullptr;
  uint16_t* last_depth_data_ = nullptr;
  bool has_camera_update_ = false;
  bool has_depth_update_ = false;
};

KinectDevice::KinectDevice(freenect_device* device) : device_(device) {
  CHECK(device_);
  int camera_data_size = DEVICE_PIXELS * 3;
  camera_data1_ = new uint8_t[camera_data_size];
  camera_data2_ = new uint8_t[camera_data_size];
  depth_data1_ = new uint16_t[DEVICE_PIXELS];
  depth_data2_ = new uint16_t[DEVICE_PIXELS];
}

KinectDevice::~KinectDevice() {
  if (device_)
    freenect_close_device(device_);

  delete[] camera_data1_;
  delete[] camera_data2_;
  delete[] depth_data1_;
  delete[] depth_data2_;
}

void KinectDevice::Setup(bool camera_enabled, bool depth_enabled) {
  CHECK_FREENECT(freenect_set_led(device_, LED_RED));
  freenect_update_tilt_state(device_);
  freenect_get_tilt_state(device_);
  // TODO(igorc): Try switching to compressed UYVY and depth streams.
  CHECK_FREENECT(freenect_set_depth_mode(
      device_, freenect_find_depth_mode(
	  FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_MM)));
  // Use YUV_RGB as it forces 15Hz refresh rate and takes 2 bytes per pixel.
  CHECK_FREENECT(freenect_set_video_mode(
      device_, freenect_find_video_mode(
	  FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_YUV_RGB)));
  CHECK_FREENECT(freenect_set_depth_buffer(device_, depth_data1_));
  CHECK_FREENECT(freenect_set_video_buffer(device_, camera_data1_));

  fprintf(stderr, "DEBUG: Before freenect_start_video\n");
  if (camera_enabled)
    CHECK_FREENECT(freenect_start_video(device_));
  fprintf(stderr, "DEBUG: Before freenect_start_depth\n");
  // TODO(igorc): Reduce the bandwith and reenable depth image.
  if (depth_enabled)
    CHECK_FREENECT(freenect_start_depth(device_));
  fprintf(stderr, "DEBUG: After freenect_start_depth\n");
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
  last_depth_data_ = reinterpret_cast<uint16_t*>(depth_data);
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

  void EnableCamera() override;
  void EnableDepth() override;
  void Start(int fps) override;

  int GetWidth() const override;
  int GetHeight() const override;
  int GetDepthDataLength() const override;
  void GetDepthData(uint8_t* dst) const override;
  void GetCameraData(uint8_t* dst) const override;

  Bytes* GetAndClearLastDepthColorImage() override;
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
  void ContrastDepth();
  bool CopyCameraDataLocked(const uint8_t* src, int device_idx);
  bool CopyDepthDataLocked(const uint16_t* src, int device_idx);

  int fps_;
  bool camera_enabled_ = false;
  bool depth_enabled_ = false;
  freenect_context* context_ = nullptr;
  pthread_t freenect_thread_;
  pthread_t merger_thread_;
  volatile bool should_exit_ = false;
  mutable pthread_mutex_t devices_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  mutable pthread_mutex_t merger_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  std::vector<KinectDevice*> devices_;
  bool has_started_thread_ = false;
  cv::Mat camera_data_;
  cv::Mat depth_data_orig_;
  cv::Mat depth_data_blur_;
  cv::Mat depth_data_range_;
  cv::Mat depth_data_range_copy_;
  cv::Mat erode_element_;
  cv::Mat dilate_element_;
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

KinectRangeImpl::KinectRangeImpl() : fps_(15) {
  erode_element_ = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  dilate_element_ = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(8, 8));
}

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
}

void KinectRangeImpl::EnableCamera() {
  CHECK(!has_started_thread_);
  camera_enabled_ = true;
}

void KinectRangeImpl::EnableDepth() {
  CHECK(!has_started_thread_);
  depth_enabled_ = true;
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
    fprintf(stderr, "Connecting to Kinect #%d\n", i);
    int openAttempt = 1;
    freenect_device* device = nullptr;
    while (true) {
      int res = freenect_open_device(context_, &device, i);
      if (!res)
	break;
      if (openAttempt >= MAX_DEVICE_OPEN_ATTEMPTS) {
	CHECK_FREENECT(res);
      }
      Sleep(0.5);
      fprintf(
	  stderr, "Retrying freenect_open_device on #%d after error %d\n",
	  i, res);
      ++openAttempt;
    }

    freenect_set_depth_callback(device, OnDepthCallback);
    freenect_set_video_callback(device, OnCameraCallback);
    KinectDevice* kinectDevice = new KinectDevice(device);
    kinectDevice->Setup(camera_enabled_, depth_enabled_);
    devices_.push_back(kinectDevice);
  }

  camera_data_.create(DEVICE_HEIGHT, DEVICE_WIDTH * device_count, CV_8UC3);
  depth_data_orig_.create(DEVICE_HEIGHT, DEVICE_WIDTH * device_count, CV_16UC1);
  depth_data_blur_.create(DEVICE_HEIGHT, DEVICE_WIDTH * device_count, CV_16UC1);
  camera_data_.setTo(cv::Scalar(0, 0, 0));
  depth_data_orig_.setTo(cv::Scalar(0));
  depth_data_blur_.setTo(cv::Scalar(0));

  CHECK(!pthread_create(&merger_thread_, NULL, RunMergerLoop, this));
  CHECK(!pthread_create(&freenect_thread_, NULL, RunFreenectLoop, this));

  fprintf(stderr, "Finished connected to Kinect devices\n");
}

// static
void* KinectRangeImpl::RunFreenectLoop(void* arg) {
  reinterpret_cast<KinectRangeImpl*>(arg)->RunFreenectLoop();
  return NULL;
}

void KinectRangeImpl::RunFreenectLoop() {
  while (!should_exit_) {
    CHECK_FREENECT(freenect_process_events(context_));
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
}

void KinectRangeImpl::HandleCameraData(
    freenect_device* dev, void* rgb_data) {
  Autolock l(devices_mutex_);
  KinectDevice* device = FindDeviceLocked(dev);
  CHECK(device);
  device->HandleCameraData(rgb_data);
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
      CopyCameraDataLocked(device->last_camera_data(), i);
      CopyDepthDataLocked(device->last_depth_data(), i);
      // TODO(igorc): Erase device's part of the image after
      // a few missing updates.
    }
  }

  if (has_depth_update) {
    ContrastDepth();
    has_new_depth_image_ = true;
  }

  if (has_camera_update)
    has_new_camera_image_ = true;
}

void KinectRangeImpl::ContrastDepth() {
  // Blur the depth image to reduce noise.
  // TODO(igorc): Try to reduce CPU usage here (using 10% now?).
  const int kernel_size = 7;
  cv::blur(
      depth_data_orig_, depth_data_blur_,
      cv::Size(kernel_size, kernel_size), cv::Point(-1,-1));

  // Select trigger pixels.
  // The depth range is approximately 3 meters. The height of the car
  // is approximately the same. We want to detect objects in the range
  // from 1 to 1.5 meters away from the Kinect.
  const uint16_t min_threshold = 1000;
  const uint16_t max_threshold = 1500;
  cv::inRange(depth_data_blur_,
	      cv::Scalar(min_threshold),
	      cv::Scalar(max_threshold),
	      depth_data_range_);

  // Further blur range image, using in-place erode-dilate.
  cv::erode(depth_data_range_, depth_data_range_, erode_element_);
  cv::erode(depth_data_range_, depth_data_range_, erode_element_);
  cv::dilate(depth_data_range_, depth_data_range_, dilate_element_);
  cv::dilate(depth_data_range_, depth_data_range_, dilate_element_);

  // Find contours of objects in the range image.
  // Use depth_data_range_copy_ as the image will be modified.
  cv::vector<cv::vector<cv::Point> > contours;
  cv::vector<cv::Vec4i> hierarchy;
  depth_data_range_.copyTo(depth_data_range_copy_);
  cv::findContours(depth_data_range_copy_, contours, hierarchy,
		   CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE);
}

bool KinectRangeImpl::CopyCameraDataLocked(
    const uint8_t* src, int device_idx) {
  if (!src) {
    // Keep original zero's in the destination array.
    return false;
  }
  uint8_t* dst = reinterpret_cast<uint8_t*>(camera_data_.data);
  if (devices_.size() == 1) {
    memcpy(dst, src, DEVICE_PIXELS * 3);
    return true;
  }
  int row_size = DEVICE_WIDTH * 3;
  uint8_t* dst_shifted = dst + row_size * device_idx;
  int dst_row_size = row_size * devices_.size();
  for (int i = 0; i < DEVICE_HEIGHT; ++i) {
    memcpy(dst_shifted + i * dst_row_size,
	   src + i * row_size, row_size);
  }
  return true;
}

bool KinectRangeImpl::CopyDepthDataLocked(
    const uint16_t* src, int device_idx) {
  if (!src) {
    // Keep original zero's in the destination array.
    return false;
  }
  // Convert 16-bit depth mm to 8-bit depth by assuming >2.55 is too far.
  uint16_t* dst = reinterpret_cast<uint16_t*>(depth_data_orig_.data);
  dst += DEVICE_WIDTH * device_idx;
  for (int i = 0; i < DEVICE_HEIGHT; ++i) {
    for (int j = 0; j < DEVICE_WIDTH; ++j) {
      uint16_t distance = src[i * DEVICE_WIDTH + j];
      // Clamp to practical limits of 0.5-3m.
      if (distance < 500) {
        distance = 500;
      } else if (distance > 3000) {
        distance = 3000;
      }
      *(dst++) = distance;
    }
    dst += DEVICE_WIDTH * (devices_.size() - 1);
  }
  return true;
}

int KinectRangeImpl::GetWidth() const {
  return DEVICE_WIDTH * devices_.size();
}

int KinectRangeImpl::GetHeight() const {
  return DEVICE_HEIGHT;
}

int KinectRangeImpl::GetDepthDataLength() const {
  return depth_data_orig_.total() * depth_data_orig_.elemSize();
}

void KinectRangeImpl::GetDepthData(uint8_t* dst) const {
  Autolock l(merger_mutex_);
  memcpy(dst, depth_data_blur_.data, GetDepthDataLength());
}

void KinectRangeImpl::GetCameraData(uint8_t* dst) const {
  Autolock l(merger_mutex_);
  memcpy(dst, camera_data_.data,
	 camera_data_.total() * camera_data_.elemSize());
}

Bytes* KinectRangeImpl::GetAndClearLastDepthColorImage() {
  Autolock l(merger_mutex_);
  if (!has_new_depth_image_)
    return NULL;

  // Expand range to 0..255.
  double min = 0;
  double max = 0;
  cv::minMaxIdx(depth_data_blur_, &min, &max);
  cv::Mat adjMap;
  double scale = 255.0 / (max - min);
  depth_data_blur_.convertTo(adjMap, CV_8UC1, scale, -min * scale);
  // depth_data_range_.copyTo(adjMap);

  // Color-code the depth map.
  cv::Mat coloredMap;
  cv::applyColorMap(adjMap, coloredMap, cv::COLORMAP_JET);

  // Convert to RGB.
  cv::Mat coloredMapRgb;
  cv::cvtColor(coloredMap, coloredMapRgb, CV_BGR2RGB);

  Bytes* result = new Bytes(
      coloredMapRgb.data,
      coloredMapRgb.total() * coloredMapRgb.elemSize());
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
  const uint8_t* src = reinterpret_cast<const uint8_t*>(camera_data_.data);
  int dst_size = width * height * 4;
  uint8_t* dst = new uint8_t[dst_size];
  for (int y = 0; y < height; ++y) {
    uint8_t* dst_row = dst + y * width * 4;
    const uint8_t* src_row = src + y * width * 3;
    for (int x = 0; x < width; ++x) {
      memcpy(dst_row + x * 4, src_row + x * 3, 3);
      dst_row[3] = 0;
    }
  }
  Bytes* result = new Bytes();
  result->MoveOwnership(dst, dst_size);
  has_new_camera_image_ = false;
  return result;
}
