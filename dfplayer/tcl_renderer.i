%module tcl_renderer_cc

%typemap(in) Bytes* {
  if (!PySequence_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    return NULL;
  }
  PyObject* seq = PySequence_Fast($input, "arg must be iterable");
  if (!seq)
    return NULL;
  Py_ssize_t len = PySequence_Fast_GET_SIZE(seq);
  PyObject** items = PySequence_Fast_ITEMS(seq);
  uint8_t* data = new uint8_t[len];
  for (Py_ssize_t i = 0; i < len; i++) {
    long value = PyInt_AsLong(items[i]);
    if (value == -1 && PyErr_Occurred()) {
      delete[] data;
      Py_XDECREF(seq);
      PyErr_SetString(PyExc_ValueError, "Expecting a sequence of ints");
      return NULL;
    }
    data[i] = value & 0xFF;
  }
  Py_DECREF(seq);
  $1 = new Bytes(data, len);
}

%typemap(freearg) Bytes* {
  delete $1;
}

%{
#include "tcl_renderer.h"
%}
 
%include "tcl_renderer.h"

