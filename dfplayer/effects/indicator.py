# -*- coding: utf-8 -*-
# Licensed under The MIT License

from ..effect import Effect, register

class Indicator(Effect):

  def __init__(self, duration=900, color='#00FF00'):
    Effect.__init__(self, duration=duration)
    self._thresh = 0
    self._rms_old = 0
    self._bass_old1 = 0
    self._bass_old2 = 0
    self._bass_old3 = 0
    self._elapsed = 0

  def get_image(self, elapsed, rms, bass):
    if self._elapsed:
      dt = elapsed - self._elapsed
    else:
      dt = 0.1
    self._elapsed = elapsed
    print '', dt
    r = rms - self._rms_old
    b1 = bass[0] - self._bass_old1
    b2 = bass[2] - self._bass_old2
    b3 = bass[4] - self._bass_old3
    self._rms_old = rms
    self._bass_old1 = bass[0]
    self._bass_old2 = bass[2]
    self._bass_old3 = bass[4]
    image = self._create_image('#000000', 255)
    #color = (int(255 * r * 10), 0, 0, 255)
    color = (int(255 * max(b1, b2, b3)), 0, 0, 255)
    image.paste(color, (0, 0, self._width, int(self._height)))
#    if b1 > 0.3 or b2 > 0.3 or b3 > 0.3:
#      color = (int(255 * rms), 0, 0, 255)
#      image.paste(color, (0, 0, self._width, int(self._height)))
#    else:
#      color = (0, int(255 * rms), 0, 255)
#      image.paste(color, (0, 0, self._width, int(self._height)))
    #else:
    #  image.paste('#00FF00', (0, 0, self._width, int(self._height * r * 10)))
    #image.paste('#00FF00', (0, 0, self._width / 2, int(self._height * r * 10)))
    #image.paste('#00FF00', (self._width / 2, 0, self._width, int(self._height * b)))
    return image


    if bass[1] > self._thresh:
      self._thresh = 2
    else:
      self._thresh = (self._thresh - 1.3) * 0.96 + 1.3
    if abs(self._thresh - 2) < 0.001:
      color = '#FF00%2d' % (self._thresh * 30)
    else:
      color = '#00FF%2d' % (self._thresh * 30)
    return self._create_image(color, 255)


register('indicator', Indicator)

