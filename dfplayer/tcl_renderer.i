%module tcl_renderer_cc

%typemap(in) Bytes* bytes {
  if (!PySequence_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    return NULL;
  }
  PyObject* seq = PySequence_Fast($input, "arg must be iterable");
  if (!seq)
    return NULL;
  Py_ssize_t len = PySequence_Fast_GET_SIZE(seq);
  PyObject** items = PySequence_Fast_ITEMS(seq);
  uint8_t* data = new uint8_t[len * 3];
  for (Py_ssize_t i = 0; i < len; i++) {
    PyObject* item = items[i];
    int pos = i * 3;
    if (!PyTuple_Check(item) || PyArg_ParseTuple(
          item, "bbb", data[pos], data[pos + 1], data[pos + 2])) {
      delete[] data;
      Py_XDECREF(seq);
      PyErr_SetString(PyExc_ValueError, "Expecting a touple of 3 colors");
      return NULL;
    }
  }
  fprintf(stderr, "Parsed %ld bytes\n", len);
  Py_DECREF(seq);
  $1 = new Bytes(data, len * 3);
}

%typemap(freearg) Bytes* bytes {
  delete $1;
}

%{
#include "tcl_renderer.h"
%}
 
%include "tcl_renderer.h"

