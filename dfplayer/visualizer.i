%module visualizer_cc

%include "typemaps.i"
%include "std_vector.i"

%typemap(in) Bytes* bytes {
  // $1 = FlattenImageData($input);
  char* buffer;
  Py_ssize_t length;
  if (PyString_AsStringAndSize($input, &buffer, &length) == -1)
    return NULL;
  uint8_t* data = new uint8_t[length];
  memcpy(data, buffer, length);
  $1 = new Bytes(data, length);
}

%typemap(freearg) Bytes* bytes {
  delete $1;
}

%{
#include "visualizer.h"
%}

%include "visualizer.h"

