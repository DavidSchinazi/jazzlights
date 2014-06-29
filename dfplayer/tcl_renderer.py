# -*- coding: utf-8 -*-
#
# Controls TCL controller.

from .tcl_layout import TclLayout
from .tcl_renderer_py import TclRenderer as TclPyImpl
from .tcl_renderer_cc import Bytes as TclCcBytes
from .tcl_renderer_cc import Layout as TclCcLayout
from .tcl_renderer_cc import TclRenderer as TclCcImpl
from .tcl_renderer_cc import Time as TclCcTime

_USE_CC_IMPL = True

class TclRenderer(object):
  """To use this class:
       renderer = TclRenderer(controller_id, width, height, 'layout.dxf', 2.4)
       # 'image_colors' has tuples with 3 RGB bytes for each pixel,
       # with 'height' number of sequential rows, each row having
       # length of 'width' pixels.
       renderer.send_frame(image_data)
  """

  def __init__(self, controller_id, width, height, layout_file, gamma):
    self._layout = TclLayout(layout_file, width - 1, height - 1)
    if _USE_CC_IMPL:
      layout = TclCcLayout()
      for s in self._layout.get_strands():
        for c in s.get_coords():
          layout.AddCoord(s.get_id(), c[0], c[1])
      self._renderer = TclCcImpl(controller_id, width, height, layout, gamma)
    else:
      self._renderer = TclPyImpl(controller_id, width, height, self._layout)
    self.set_gamma(gamma)

  def get_layout_coords(self):
    return self._layout.get_all_coords()

  def set_gamma(self, gamma):
    # 1.0 is uncorrected gamma, which is perceived as "too bright"
    # in the middle. 2.4 is a good starting point. Changing this value
    # affects mid-range pixels - higher values produce dimmer pixels.
    self.set_gamma_ranges((0, 255, gamma), (0, 255, gamma), (0, 255, gamma))

  def set_gamma_ranges(self, r, g, b):
    if _USE_CC_IMPL:
      self._renderer.SetGammaRanges(
          r[0], r[1], r[2], g[0], g[1], g[2], b[0], b[1], b[2])
    else:
      self._renderer.set_gamma_ranges(r, g, b)

  def send_frame(self, image_data):
    if _USE_CC_IMPL:
      time = TclCcTime()
      bytes = TclCcBytes(image_data)
      self._renderer.ScheduleImageAt(bytes, time)
    else:
      self._renderer.send_frame(list(image_data))

