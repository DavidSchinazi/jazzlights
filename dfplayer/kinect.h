// Copyright 2015, Igor Chernyshev.
// Licensed under The MIT License
//

#ifndef __DFPLAYER_KINECT_H
#define __DFPLAYER_KINECT_H

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"
#include "../libfreenect/include/libfreenect.h"

// Contains array of bytes and deallocates in destructor.
struct KinectConnection {
  KinectConnection();
  ~KinectConnection();

  void Start(int device_id);

 private:
  KinectConnection(const KinectConnection& src);
  KinectConnection& operator=(const KinectConnection& rhs);

  static void* RunFreenectLoop(void* arg);
  static void OnDepthCallback(
      freenect_device* dev, void* depth_data, uint32_t timestamp);
  static void OnCameraCallback(
      freenect_device* dev, void* rgb_data, uint32_t timestamp);

  void RunFreenectLoop();
  void HandleDepthData(freenect_device* dev, void* depth_data);
  void HandleCameraData(freenect_device* dev, void* rgb_data);

  freenect_context* context_;
  freenect_device* device_ = nullptr;
  pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
  pthread_t looper_thread_;
  volatile bool should_exit_ = false;
  uint8_t* camera_data1_;
  uint8_t* camera_data2_;
  uint8_t* depth_data1_;
  uint8_t* depth_data2_;
  uint8_t* last_camera_data_ = nullptr;
  uint8_t* last_depth_data_ = nullptr;
};

#endif  // __DFPLAYER_KINECT_H
