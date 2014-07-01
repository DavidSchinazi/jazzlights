# -*- coding: utf-8 -*-
#
# Loads and builds frame images.

import os

from PIL import Image

from .effect import load as load_effect
from .util import get_time_millis

class FrameSource(object):

  def __init__(self, fps, screen_width, frame_width, frame_height):
    self._fps = fps
    self._screen_width = screen_width
    self._frame_width = frame_width
    self._frame_height = frame_height
    self._effect = None
    self._split_sides = True

  def play_effect(self, name, **kwargs):
    self._effect = load_effect(name, **kwargs)

  def stop_effect(self):
    self._effect = None

  def toggle_split_sides(self):
    self._split_sides = not self._split_sides

  def load_frame_file(self, path):
    # TODO(igorc): Implement ahead-of-time caching and scheduling.
    return self._load_frame_file(path, get_time_millis())

  def _load_frame_file(self, path, current_ms):
    if not os.path.exists(path):
      # TODO(igorc): Handle the case where '_frame' was never loaded.
      return None

    with open(path, 'rb') as imgfile:
      img = Image.open(imgfile)
      img.load()
    if img.size[0] != self._frame_width or img.size[1] != self._frame_height:
      print 'Unexpected image size for %s = %s' % (path, [img.size])
      return None

    if self._effect is not None:
      if not self._effect(img, current_ms):
        self._effect = None

    if self._screen_width / self._frame_width == 2:
      # Pre-split images, just copy them twice.
      frame = Image.new(
          'RGB', (self._screen_width, self._frame_height))
      frame.paste(img, (0, 0))
      frame.paste(img, (self._frame_width, 0))
    elif self._split_sides:
      sub_img = img.resize((img.size[0] / 2, img.size[1]))
      #sub_img = img.crop((
      #    img.size[0] / 4, 0, img.size[0] * 3 / 4, img.size[1]))
      frame = Image.new('RGB', img.size)
      frame.paste(sub_img, (0, 0))
      frame.paste(sub_img, (sub_img.size[0], 0))
    else:
      frame = img
    return frame

