# -*- coding: utf-8 -*-

from PIL import Image, ImageFont, ImageDraw

from ..util import PROJECT_DIR
from ..player import FRAME_HEIGHT
from ..effect import Effect, register


class TextTicker(Effect):

  def __init__(self, text='HELLO', duration=7, color='white'):
    Effect.__init__(self, duration=duration, mirror=False)
    self._text = text
    self._color = color

  def _prepare(self):
    font = ImageFont.truetype(PROJECT_DIR + '/dfplayer/effects/impact.ttf',
        int(self._height * 0.9))
    (w, h) = font.getsize(self._text)
    self._rendered_text = Image.new(
        'RGBA', (int(w * 1.6), self._height), 'black')
    draw = ImageDraw.Draw(self._rendered_text)
    draw.text((int(w * 0.3), int(self._height * 0.1)),
              self._text, self._color, font=font)

  def get_image(self, elapsed):
    image = self._create_image('black', 255)
    frame_width = image.size[0]
    text_width = self._rendered_text.size[0]
    x_ratio = (self._duration - elapsed) / self._duration
    if x_ratio < 0:
      x_ratio = 0
    x = (text_width + frame_width) * x_ratio - text_width
    image.paste(self._rendered_text, (int(x), 0))
    return image


register('textticker', TextTicker)
