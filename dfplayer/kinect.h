// Copyright 2015, Igor Chernyshev.
// Licensed under The MIT License
//

#ifndef __DFPLAYER_KINECT_H
#define __DFPLAYER_KINECT_H

#include "utils.h"

class KinectRange {
 public:
  static KinectRange* GetInstance();

  KinectRange() {}
  virtual ~KinectRange() {}

  virtual void Start(int fps) = 0;

  // These functions are only valid after Start().
  virtual int GetWidth() const = 0;
  virtual int GetHeight() const = 0;
  virtual void GetDepthData(uint8_t* dst) const = 0;
  virtual void GetCameraData(uint8_t* dst) const = 0;

  virtual Bytes* GetAndClearLastDepthImage() = 0;
  virtual Bytes* GetAndClearLastCameraImage() = 0;

 private:
  KinectRange(const KinectRange& src);
  KinectRange& operator=(const KinectRange& rhs);

  static KinectRange* instance_;
};

#endif  // __DFPLAYER_KINECT_H
