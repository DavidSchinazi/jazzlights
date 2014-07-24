# -*- coding: utf-8 -*-

from ..effect import FixedDurationEffect, register


class TestStripes(FixedDurationEffect):

  def __init__(
      self, duration=60, fgcolor='grey', bgcolor='black', thickness=4):
    FixedDurationEffect.__init__(self, duration=duration)
    self._fgcolor = fgcolor
    self._bgcolor = bgcolor
    self._thickness = thickness

  def _apply(self, frame, elapsed):
    (w, h) = frame.size
    frame.paste(self._bgcolor, (0, 0, w, h))
    if elapsed < self._duration / 2:
      x = int(2 * w * elapsed / self._duration)
      frame.paste(self._fgcolor, (x, 0, x + self._thickness, h))
    else:
      y = int(2 * h * (elapsed - self._duration / 2) / self._duration)
      frame.paste(self._fgcolor, (0, y, w, y + self._thickness))


register('teststripes', TestStripes)
