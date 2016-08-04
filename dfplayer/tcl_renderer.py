# -*- coding: utf-8 -*-
# Licensed under The MIT License
#
# Controls TCL controller.

from PIL import Image

from .stats import Stats
from .tcl_layout import TclLayout
from .renderer_cc import LedLayout as TclCcLayout
from .renderer_cc import TclRenderer as TclCcImpl
from .renderer_cc import AdjustableTime as TclCcTime
from .util import get_time_millis

class TclRenderer(object):
  def __init__(self, fps, enable_net, test_mode=False):
    self._fps = fps
    self._enable_net = enable_net
    self._test_mode = test_mode
    self._hdr_mode = 2  # Saturation only
    self._widths = {}
    self._heights = {}

  def add_controller(self, controller_id, width, height, gamma):
    self._widths[controller_id] = width
    self._heights[controller_id] = height
    layout_file = 'dfplayer/layout%d.dxf' % controller_id
    layout_src = TclLayout(layout_file, width - 1, height - 1)
    layout = TclCcLayout()
    for s in layout_src.get_strands():
      for c in s.get_coords():
        layout.AddCoord(s.get_id(), c[0], c[1])
    self._renderer = TclCcImpl.GetInstance()
    self._renderer.AddController(controller_id, width, height, layout, gamma)

  def lock_controllers(self):
    self._renderer.LockControllers()
    self._frame_send_duration = self._renderer.GetFrameSendDuration()
    self._renderer.SetHdrMode(self._hdr_mode)
    if not self._test_mode:
      self._renderer.StartMessageLoop(self._fps, self._enable_net)
    self._frame_delays = []
    self._frame_delays_clear_time = get_time_millis()

  def get_width(self, controller_id):
    return self._widths[controller_id]

  def get_height(self, controller_id):
    return self._heights[controller_id]

  def set_gamma(self, gamma):
    # 1.0 is uncorrected gamma, which is perceived as "too bright"
    # in the middle. 2.4 is a good starting point. Changing this value
    # affects mid-range pixels - higher values produce dimmer pixels.
    self.set_gamma_ranges((0, 255, gamma), (0, 255, gamma), (0, 255, gamma))

  def set_gamma_ranges(self, r, g, b):
    self._renderer.SetGammaRanges(
        r[0], r[1], r[2], g[0], g[1], g[2], b[0], b[1], b[2])

  def disable_reset(self):
    self._renderer.SetAutoResetAfterNoDataMs(0)

  def has_scheduling_support(self):
    return self._use_cc_impl

  def get_and_clear_last_image(self, controller):
    img_data = self._renderer.GetAndClearLastImage(controller)
    if not img_data or len(img_data) == 0:
      return None
    return Image.fromstring(
        'RGBA', (self._widths[controller], self._heights[controller]),
        img_data)

  def get_and_clear_last_led_image(self, controller):
    img_data = self._renderer.GetAndClearLastLedImage(controller)
    if not img_data or len(img_data) == 0:
      return None
    return Image.fromstring(
        'RGBA', (self._widths[controller], self._heights[controller]),
        img_data)

  def get_queue_size(self):
    return self._renderer.GetQueueSize()

  def get_init_status(self):
    return self._renderer.GetInitStatus()

  def reset_image_queue(self):
    self._renderer.ResetImageQueue()

  def set_wearable_effect(self, id):
    self._renderer.SetWearableEffect(id)

  def enable_rainbow(self, controller, x):
    self._renderer.EnableRainbow(controller, x)

  def set_effect_image(self, controller, image, mirror):
    if image:
      self._renderer.SetEffectImage(
          controller, image.tostring(),
          image.size[0], image.size[1], 2 if mirror else 1)
    else:
      self._renderer.SetEffectImage(controller, '', 0, 0, 2)

  def toggle_hdr_mode(self):
    self._hdr_mode += 1
    self._hdr_mode %= 4
    self._renderer.SetHdrMode(self._hdr_mode)

  def get_hdr_mode(self):
    return self._hdr_mode

  def get_frame_data_for_test(self, controller, image):
    return self._renderer.GetFrameDataForTest(
        controller, image.tostring())

  def get_and_clear_frame_delays(self):
    self._populate_frame_delays()
    result = self._frame_delays
    duration = get_time_millis() - self._frame_delays_clear_time
    self._frame_delays = []
    self._frame_delays_clear_time = get_time_millis()
    return (duration, result)

  def _populate_frame_delays(self):
    for d in self._renderer.GetAndClearFrameDelays():
      self._frame_delays.append(d)
