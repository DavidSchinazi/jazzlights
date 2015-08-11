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

#include "../external/kkonnect/include/kk_connection.h"
#include "../external/kkonnect/include/kk_device.h"

using kkonnect::Connection;
using kkonnect::Device;
using kkonnect::DeviceOpenRequest;
using kkonnect::ErrorCode;
using kkonnect::ImageInfo;

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


class KinectRangeImpl : public KinectRange {
 public:
  KinectRangeImpl();
  ~KinectRangeImpl() override;

  void EnableVideo() override;
  void EnableDepth() override;
  void Start(int fps) override;

  int GetWidth() const override;
  int GetHeight() const override;
  int GetDepthDataLength() const override;
  void GetDepthData(uint8_t* dst) const override;
  void GetVideoData(uint8_t* dst) const override;

  Bytes* GetAndClearLastDepthColorImage() override;
  Bytes* GetAndClearLastVideoImage() override;

 private:
  static KinectRangeImpl* GetInstanceImpl();

  void ConnectDevices();

  static void* RunMergerLoop(void* arg);
  void RunMergerLoop();
  void MergeImages();
  void ContrastDepth();
  void ClampDepthData();

  int fps_;
  bool video_enabled_ = false;
  bool depth_enabled_ = false;
  Connection* connection_ = nullptr;
  pthread_t merger_thread_;
  volatile bool should_exit_ = false;
  mutable pthread_mutex_t devices_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  mutable pthread_mutex_t merger_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  std::vector<Device*> devices_;
  int width_ = 0;
  int height_ = 0;
  bool has_started_thread_ = false;
  cv::Mat video_data_;
  cv::Mat depth_data_orig_;
  cv::Mat depth_data_blur_;
  cv::Mat depth_data_range_;
  cv::Mat depth_data_range_copy_;
  cv::Mat erode_element_;
  cv::Mat dilate_element_;
  bool has_new_depth_image_ = false;
  bool has_new_video_image_ = false;
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
  pthread_join(merger_thread_, NULL);

  if (connection_)
    connection_->Close();
}

void KinectRangeImpl::EnableVideo() {
  CHECK(!has_started_thread_);
  video_enabled_ = true;
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

  ConnectDevices();

  CHECK(!pthread_create(&merger_thread_, NULL, RunMergerLoop, this));
}

void KinectRangeImpl::ConnectDevices() {
  connection_ = Connection::OpenLocal();

  int device_count = connection_->GetDeviceCount();
  fprintf(stderr, "Found %d Kinect devices\n", device_count);

  Device* device = nullptr;
  DeviceOpenRequest request(0);
  if (video_enabled_)
    request.depth_format = kkonnect::kImageFormatVideoRgb;
  if (depth_enabled_)
    request.depth_format = kkonnect::kImageFormatDepthMm;
  ErrorCode err = connection_->OpenDevice(request, &device);
  if (err != kkonnect::kErrorSuccess) {
    fprintf(stderr, "Failed to open Kinect device, error=%d\n", err);
    return;
  }

  uint64_t start_time = GetCurrentMillis();
  while (device->GetStatus() == kkonnect::kErrorInProgress) {
    uint64_t elapsed_ms = GetCurrentMillis() - start_time;
    if (elapsed_ms > 15 * 1000) {
      fprintf(stderr, "Timed out waiting for a Kinect connection\n");
      connection_->CloseDevice(device);
      return;
    }
    Sleep(0.1);
  }

  err = device->GetStatus();
  if (err != kkonnect::kErrorSuccess) {
    fprintf(stderr, "Failed to connect to Kinect device, error=%d\n", err);
    return;
  }

  ImageInfo video_info = device->GetVideoImageInfo();
  ImageInfo depth_info = device->GetDepthImageInfo();
  if (!video_info.enabled && !depth_info.enabled) {
    fprintf(stderr, "Both video and depth streams are closed\n");
  } else if (video_info.enabled && depth_info.enabled) {
    CHECK(video_info.width == depth_info.width);
    CHECK(video_info.height == depth_info.height);
  }

  if (video_info.enabled) {
    width_ = video_info.width;
    height_ = video_info.height;
  } else if (depth_info.enabled) {
    width_ = depth_info.width;
    height_ = depth_info.height;
  }

  CHECK(width_ > 0);
  CHECK(height_ > 0);

  devices_.push_back(device);

  // TODO(igorc): Get width and height.

  video_data_.create(height_, width_ * device_count, CV_8UC3);
  video_data_.setTo(cv::Scalar(0, 0, 0));

  depth_data_orig_.create(height_, width_ * device_count, CV_16UC1);
  depth_data_blur_.create(height_, width_ * device_count, CV_16UC1);
  depth_data_orig_.setTo(cv::Scalar(0));
  depth_data_blur_.setTo(cv::Scalar(0));
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
  bool has_video_update = false;
  {
    // Merge images from all devices into one.
    Autolock l2(devices_mutex_);
    for (size_t i = 0; i < devices_.size(); ++i) {
      Device* device = devices_[i];
      int full_witdh = width_ * devices_.size();
      has_depth_update |= device->GetAndClearDepthData(
	  reinterpret_cast<uint16_t*>(depth_data_orig_.data),
	  full_witdh * 2);
      has_video_update |= device->GetAndClearVideoData(
	  video_data_.data, full_witdh * 3);
      // TODO(igorc): Erase device's part of the image after
      // a few missing updates.
    }
  }

  if (has_depth_update) {
    ContrastDepth();
    has_new_depth_image_ = true;
  }

  if (has_video_update)
    has_new_video_image_ = true;
}

void KinectRangeImpl::ContrastDepth() {
  ClampDepthData();

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

void KinectRangeImpl::ClampDepthData() {
  CHECK(depth_data_orig_.elemSize() == 2);
  uint16_t* data = reinterpret_cast<uint16_t*>(depth_data_orig_.data);
  for (uint32_t i = 0; i < video_data_.total(); ++i) {
    // Clamp to practical limits of 0.5-3m.
    uint16_t distance = data[i];
    if (distance < 500) {
      data[i] = 500;
    } else if (distance > 3000) {
      data[i]= 3000;
    }
  }
}

int KinectRangeImpl::GetWidth() const {
  return width_ * devices_.size();
}

int KinectRangeImpl::GetHeight() const {
  return height_;
}

int KinectRangeImpl::GetDepthDataLength() const {
  return depth_data_orig_.total() * depth_data_orig_.elemSize();
}

void KinectRangeImpl::GetDepthData(uint8_t* dst) const {
  Autolock l(merger_mutex_);
  memcpy(dst, depth_data_blur_.data, GetDepthDataLength());
}

void KinectRangeImpl::GetVideoData(uint8_t* dst) const {
  Autolock l(merger_mutex_);
  memcpy(dst, video_data_.data,
	 video_data_.total() * video_data_.elemSize());
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

Bytes* KinectRangeImpl::GetAndClearLastVideoImage() {
  Autolock l(merger_mutex_);
  if (!has_new_video_image_)
    return NULL;
  // Unpack 3-byte RGB into RGBA.
  int width = GetWidth();
  int height = GetHeight();
  const uint8_t* src = reinterpret_cast<const uint8_t*>(video_data_.data);
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
  has_new_video_image_ = false;
  return result;
}
