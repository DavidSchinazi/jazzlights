%module visualizer_cc

%include "std_string.i"
%include "typemaps.i"

%typemap(out) Bytes* {
  if ($1) {
    $result = PyString_FromStringAndSize((const char*) $1->GetData(), $1->GetLen());
    delete $1;
  } else {
    $result = Py_None;
  }
}

%{
#include "visualizer.h"
%}

%include "visualizer.h"

