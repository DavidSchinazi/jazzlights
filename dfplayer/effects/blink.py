# -*- coding: utf-8 -*-
# Licensed under The MIT License

from ..effect import Effect, register

class Blink(Effect):

  def __init__(self, duration=60, fgcolor='orange', bgcolor='black', freq=8):
    Effect.__init__(self, duration=duration)
    self._fgcolor = fgcolor
    self._bgcolor = bgcolor
    self._freq = freq

  def get_image(self, elapsed, **kwargs):
    phase = int(elapsed * self._freq) % 2
    if phase:
      return self._create_image(self._fgcolor, 255)
    else:
      return self._create_image(self._bgcolor, 255)


register('blink', Blink)
