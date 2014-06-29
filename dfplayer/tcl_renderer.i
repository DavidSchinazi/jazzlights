%module tcl_renderer_cc

%typemap(in) Bytes* bytes {
  $1 = FlattenImageData($input);
}

%typemap(freearg) Bytes* bytes {
  delete $1;
}

%{
#include "tcl_renderer.h"
%}

%inline %{
static Bytes* FlattenImageData(PyObject* input) {
  if (!PySequence_Check(input)) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    return NULL;
  }
  PyObject* input_seq = PySequence_Fast(input, "arg must be iterable");
  if (!input_seq)
    return NULL;
  Py_ssize_t len = PySequence_Fast_GET_SIZE(input_seq);
  PyObject** colors = PySequence_Fast_ITEMS(input_seq);
  uint8_t* data = new uint8_t[len * 3];
  for (Py_ssize_t i = 0; i < len; i++) {
    PyObject* color_seq = PySequence_Fast(colors[i], "color must be iterable");
    if (!color_seq)
      goto err;
    if (PySequence_Fast_GET_SIZE(color_seq) != 3) {
      PyErr_SetString(PyExc_ValueError, "Color should be a sequence of 3");
      goto err;
    }
    int pos = i * 3;
    PyObject** color_comps = PySequence_Fast_ITEMS(color_seq);
    for (int j = 0; j < 3; j++) {
      long comp = PyInt_AsLong(color_comps[j]);
      if (comp == -1 && PyErr_Occurred()) {
        Py_DECREF(color_seq);
        goto err;
      }
      if (comp < 0 || comp > 255) {
        PyErr_SetString(PyExc_ValueError, "Color component is not a byte");
        Py_DECREF(color_seq);
        goto err;
      }
      data[pos + j] = comp & 0xFF;
    }
    Py_DECREF(color_seq);
  }
  Py_DECREF(input_seq);
  return new Bytes(data, len * 3);

err:
  delete[] data;
  Py_XDECREF(input_seq);
  return NULL;
}
%}

%include "tcl_renderer.h"

