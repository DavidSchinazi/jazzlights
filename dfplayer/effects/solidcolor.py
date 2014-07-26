# -*- coding: utf-8 -*-

from ..effect import FixedDurationEffect, register

class SolidColor(FixedDurationEffect):

  def __init__(self, duration=20, color='#00FF00'):
    FixedDurationEffect.__init__(self, duration=duration)
    self._color = color

  def _apply(self, frame, elapsed):
    frame.paste(self._color, (0, 0, frame.size[0], frame.size[1]))


register('solidcolor', SolidColor)
