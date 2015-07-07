// Copyright 2015, Igor Chernyshev.
// Licensed under The MIT License
//

#include "kinect.h"

KinectConnection::KinectConnection() {
  CHECK(freenect_init(&context_, NULL) >= 0);

  freenect_set_log_level(context_, FREENECT_LOG_DEBUG);
  freenect_select_subdevices(
      context_, (freenect_device_flags) FREENECT_DEVICE_CAMERA);

  int device_count = freenect_num_devices(context_);
  fprintf(stderr, "Found %d Kinect devices\n", device_count);

  int data_size = 640 * 480 * 3;
  camera_data1_ = new uint8_t[data_size];
  camera_data2_ = new uint8_t[data_size];
  depth_data1_ = new uint8_t[data_size];
  depth_data2_ = new uint8_t[data_size];

  CHECK(pthread_create(&looper_thread_, NULL, RunFreenectLoop, NULL));
}

KinectConnection::~KinectConnection() {
  should_exit_ = true;
  pthread_join(looper_thread_, NULL);

  if (device_)
    freenect_close_device(device_);

  freenect_shutdown(context_);

  delete[] camera_data1_;
  delete[] camera_data2_;
  delete[] depth_data1_;
  delete[] depth_data2_;
}

void KinectConnection::Start(int device_id) {
  CHECK(!device_);
  CHECK(freenect_open_device(context_, &device_, device_id) >= 0);
}

// static
void* KinectConnection::RunFreenectLoop(void* arg) {
  reinterpret_cast<KinectConnection*>(arg)->RunFreenectLoop();
  return NULL;
}

void KinectConnection::RunFreenectLoop() {
  // TODO(igorc): Check call results.
  freenect_set_led(device_, LED_RED);
  freenect_set_depth_callback(device_, OnDepthCallback);
  freenect_set_video_callback(device_, OnCameraCallback);
  // http://openkinect.org/wiki/Protocol_Documentation#RGB_Camera
  // !!! 0: uncompressed 16 bit depth stream between 0x0000 and 0x07ff
  // (2^11-1). Causes bandwidth issues; will drop packets.
  freenect_set_depth_mode(
      device_, freenect_find_depth_mode(
	  FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
  freenect_set_video_mode(
      device_, freenect_find_video_mode(
	  FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB));
  freenect_set_depth_buffer(device_, depth_data1_);
  freenect_set_video_buffer(device_, camera_data1_);

  freenect_start_depth(device_);
  freenect_start_video(device_);

  while (!should_exit_ && freenect_process_events(context_) >= 0) {
    // Let callbacks do the work.
  }

  freenect_stop_depth(device_);
  freenect_stop_video(device_);
}

// static
void KinectConnection::OnDepthCallback(
    freenect_device* dev, void* depth_data, uint32_t timestamp) {
  (void) dev;
  (void) depth_data;
  (void) timestamp;
  // TODO(igorc): Call "handle" function.
}

// static
void KinectConnection::OnCameraCallback(
    freenect_device* dev, void* rgb_data, uint32_t timestamp) {
  (void) dev;
  (void) rgb_data;
  (void) timestamp;
}

void KinectConnection::HandleDepthData(
    freenect_device* dev, void* depth_data) {
  Autolock l(mutex_);
  CHECK(dev == device_);
  last_depth_data_ = reinterpret_cast<uint8_t*>(depth_data);
  freenect_set_depth_buffer(
      device_,
      last_depth_data_ == depth_data1_ ? depth_data2_ : depth_data2_);
  /*uint16_t* depth = reinterpret_cast<uint16_t*>(depth_data);
  for (int i=0; i < 640 * 480; ++i) {
    int value = depth[i];
  }*/
  // pthread_cond_signal(&cond_);
}

void KinectConnection::HandleCameraData(
    freenect_device* dev, void* rgb_data) {
  Autolock l(mutex_);
  CHECK(dev == device_);
  last_camera_data_ = reinterpret_cast<uint8_t*>(rgb_data);
  freenect_set_video_buffer(
      device_,
      last_camera_data_ == camera_data1_ ? camera_data2_ : camera_data2_);
  // pthread_cond_signal(&cond_);
}
