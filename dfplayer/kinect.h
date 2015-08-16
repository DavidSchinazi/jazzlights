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

  virtual void EnableVideo() = 0;
  virtual void EnableDepth() = 0;
  virtual void Start(int fps) = 0;

  // These functions are only valid after Start().
  virtual int GetWidth() const = 0;
  virtual int GetHeight() const = 0;
  virtual int GetDepthDataLength() const = 0;
  virtual void GetDepthData(uint8_t* dst) const = 0;
  virtual void GetVideoData(uint8_t* dst) const = 0;

  virtual Bytes* GetAndClearLastDepthColorImage() = 0;
  virtual Bytes* GetAndClearLastVideoImage() = 0;

  // Position of detected person, in the range of [0, 1).
  // Returns negative value when there is no person.
  virtual double GetPersonCoordX() const = 0;

 private:
  KinectRange(const KinectRange& src);
  KinectRange& operator=(const KinectRange& rhs);

  static KinectRange* instance_;
};

#endif  // __DFPLAYER_KINECT_H
