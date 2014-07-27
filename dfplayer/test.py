#!/usr/bin/python
#
# Test.

import argparse

from PIL import Image

from .tcl_renderer import TclRenderer

FRAME_WIDTH = 500
FRAME_HEIGHT = 50


def _get_test_frame(image, use_cc_tcl):
  tcl = TclRenderer(
      17, FRAME_WIDTH, FRAME_HEIGHT,
      'dfplayer/layout.dxf', 1, use_cc_tcl, test_mode=True)
  return tcl.get_frame_data_for_test(image)


def _test_tcl_render():
  image = Image.new('RGBA', (FRAME_WIDTH, FRAME_HEIGHT))
  image.paste('red', (0, 0, FRAME_WIDTH / 2, FRAME_HEIGHT / 2))
  data_py = _get_test_frame(image, False)
  data_cc = _get_test_frame(image, True)
  print 'Frame data length = %s' % len(data_cc)
  if len(data_py) != len(data_cc):
    raise Exception('Python length differs: %s' % len(data_py))
  has_mismatch = False
  for i in xrange(0, len(data_cc)):
    if data_py[i] != data_cc[i]:
      has_mismatch = True
      print 'Data mismatch at %s. PY=%s, CC=%s' % (
          i, data_py[i], data_cc[i])
  if has_mismatch:
    raise Exception('Test failed')


def main():
  _test_tcl_render()
  print 'Test Completed'

