# -*- coding: utf-8 -*-

import random

from ..effect import FixedDurationEffect, register


class RandomPixels(FixedDurationEffect):

  def __init__(self, duration=5):
    FixedDurationEffect.__init__(self, duration=duration)

  def _apply(self, frame, elapsed):
    def random_pixels(size):
      for i in xrange(size[0] * size[1]):
        yield (random.randint(0, 255),
            random.randint(0, 255), random.randint(0, 255))

    frame.putdata(list(random_pixels(frame.size)))


register('randompixels', RandomPixels)
