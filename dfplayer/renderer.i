%module renderer_cc

%include "std_string.i"
%include "std_vector.i"
%include "typemaps.i"

namespace std {
  %template(vectori) vector<int>;
  %template(vectord) vector<double>;
};

%newobject Visualizer::GetAndClearImage();

%typemap(out) Bytes* {
  if ($1) {
    $result = PyString_FromStringAndSize((const char*) $1->GetData(), $1->GetLen());
    delete $1;
  } else {
    $result = Py_None;
  }
}

%typemap(in) Bytes* bytes {
  char* buffer;
  Py_ssize_t length;
  if (PyString_AsStringAndSize($input, &buffer, &length) == -1)
    return NULL;
  $1 = new Bytes(buffer, length);
}

%typemap(freearg) Bytes* bytes {
  delete $1;
}

%{
#include "tcl_renderer.h"
#include "visualizer.h"
%}

%inline %{
%}

%include "tcl_renderer.h"
%include "visualizer.h"

