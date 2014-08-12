# -*- coding: utf-8 -*-
# Licensed under The MIT License

from ..effect import Effect, register


class TestStripes(Effect):

  def __init__(
      self, duration=60, fgcolor='grey', bgcolor='black', thickness=4):
    Effect.__init__(self, duration=duration)
    self._fgcolor = fgcolor
    self._bgcolor = bgcolor
    self._thickness = thickness

  def get_image(self, elapsed):
    image = self._create_image(self._bgcolor, 255)
    if elapsed < self._duration / 2:
      x = int(2 * self._width * elapsed / self._duration)
      image.paste(self._fgcolor, (x, 0, x + self._thickness, self._height))
    else:
      y = int(2 * self._height * (elapsed - self._duration / 2) / self._duration)
      image.paste(self._fgcolor, (0, y, self._width, y + self._thickness))
    return image


register('teststripes', TestStripes)
