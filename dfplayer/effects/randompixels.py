# -*- coding: utf-8 -*-

import random

from ..effect import Effect, register


class RandomPixels(Effect):

  def __init__(self, duration=7):
    Effect.__init__(self, duration=duration)

  def get_image(self, elapsed):
    def random_pixels(size):
      for i in xrange(size[0] * size[1]):
        yield (random.randint(0, 255),
            random.randint(0, 255), random.randint(0, 255))

    image = self._create_image('black', 255)
    image.putdata(list(random_pixels(image.size)))
    return image


register('randompixels', RandomPixels)
