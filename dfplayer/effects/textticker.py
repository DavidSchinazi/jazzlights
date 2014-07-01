# -*- coding: utf-8 -*-

from PIL import Image, ImageFont, ImageDraw

from ..util import PROJECT_DIR
from ..player import FRAME_HEIGHT
from ..effect import FixedDurationEffect, register


class TextTicker(FixedDurationEffect):

  def __init__(self, text='HELLO', duration=5,
               height=FRAME_HEIGHT, color='white'):
    FixedDurationEffect.__init__(self, duration=duration)
    font = ImageFont.truetype(PROJECT_DIR + '/dfplayer/effects/impact.ttf',
        int(height * 0.9))
    (w, h) = font.getsize(text)
    self._rendered_text = Image.new('RGB', (int(w * 1.6), height), 'black')
    draw = ImageDraw.Draw(self._rendered_text)
    draw.text((int(w * 0.3), int(height * 0.1)), text, color, font=font)

  def _apply(self, frame, elapsed):
    frame_width = frame.size[0]
    text_width = self._rendered_text.size[0]
    frame.paste(self._rendered_text, 
        (int((text_width + frame_width) * elapsed / self._duration - text_width), 0))


register('textticker', TextTicker)
