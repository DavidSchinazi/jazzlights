# -*- coding: utf-8 -*-
# Licensed under The MIT License

from PIL import Image, ImageFont, ImageDraw

from ..util import create_image
from ..util import PROJECT_DIR
from ..effect import Effect, register

_X_STEP = 2


class TextTicker(Effect):

  def __init__(self, text='HELLO', duration=10, color='white', alpha=185):
    Effect.__init__(self, duration=duration, mirror=False)
    self._text = text
    self._color = color
    self._alpha = alpha

  def _prepare(self):
    # More founts around: '/usr/share/fonts/truetype/'
    font = ImageFont.truetype(
        PROJECT_DIR + '/dfplayer/effects/DejaVuSans-Bold.ttf',
        int(self._height * 1))
    (w, h) = font.getsize(self._text)
    self._rendered_text = create_image(
        int(w * 1.6), self._height, 'black', self._alpha)
    draw = ImageDraw.Draw(self._rendered_text)
    draw.text((int(w * 0.2), -1), self._text, self._color, font=font)

  def get_image(self, elapsed, **kwargs):
    image = self._create_image('black', self._alpha)
    frame_width = image.size[0]
    text_width = self._rendered_text.size[0]
    x_ratio = (self._duration - elapsed) / self._duration
    if x_ratio < 0:
      x_ratio = 0
    x = (text_width + frame_width) * x_ratio - text_width
    x = _X_STEP * int(x / _X_STEP)
    image.paste(self._rendered_text, (int(x), 0))
    return image


register('textticker', TextTicker)
