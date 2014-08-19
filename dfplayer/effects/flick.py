# -*- coding: utf-8 -*-
# Licensed under The MIT License

import os

from ..effect import Effect, register

from PIL import Image


class Flick(Effect):

  def __init__(self, name='discofish', fps=15):
    Effect.__init__(self)
    self._path = os.path.dirname(os.path.dirname(os.path.dirname(__file__))) + '/env/clips/' + name
    self._fps = fps
    self._frame = None

  def get_image(self, elapsed, **kwargs):
    frame_num = 1 + int(elapsed * self._fps)
    frame_file = '%s/frame%06d.jpg' \
        % (self._path, frame_num)
    if os.path.exists(frame_file):
        with open(frame_file, 'rb') as imgfile:
            print frame_file
            image = self._create_image('#000000', 255)
            img = Image.open(imgfile)
            img.load()
            image.paste(img,(0,0))
            self._frame = image
    return self._frame


register('flick', Flick)
