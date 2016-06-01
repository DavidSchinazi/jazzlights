// Copyright 2016, Igor Chernyshev.

#ifndef TCL_TCL_TYPES_H_
#define TCL_TCL_TYPES_H_

enum InitStatus {
  INIT_STATUS_UNUSED,
  INIT_STATUS_OK,
  INIT_STATUS_FAIL,
  INIT_STATUS_RESETTING,
};

enum HdrMode {
  HDR_MODE_NONE = 0,
  HDR_MODE_LUM = 1,
  HDR_MODE_SAT = 2,
  HDR_MODE_LUM_SAT = 3,
};

#endif  // TCL_TCL_TYPES_H_
