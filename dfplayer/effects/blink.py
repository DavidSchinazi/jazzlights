# -*- coding: utf-8 -*-

from ..effect import FixedDurationEffect, register

class Blink(FixedDurationEffect):

  def __init__(self, duration=60, fgcolor='orange', bgcolor='grey', freq=8):
    FixedDurationEffect.__init__(self, duration=duration)
    self._fgcolor = fgcolor
    self._bgcolor = bgcolor
    self._freq = freq

  def _apply(self, frame, elapsed):
    (w, h) = frame.size
    phase = int(elapsed * self._freq) % 2
    if phase:
      frame.paste(self._fgcolor, (0, 0, w, h))
    else:
      frame.paste(self._bgcolor, (0, 0, w, h))


register('blink', Blink)
