# -*- coding: utf-8 -*-

import logging
import os
import sys
import time

from PIL import Image
from PIL import ImageColor

from .util import get_time_millis

_EFFECTS = {}


class Effect(object):

  def __init__(self, duration=None, mirror=True):
    self._duration = duration
    self._mirror = mirror
    self._start_ms = get_time_millis()

  def _set_dimensions(self, w, h):
    self._width = w
    self._height = h

  def _prepare(self):
    pass  # Override in the subclasses.

  def should_mirror(self):
    return self._mirror

  def get_elapsed_sec(self):
    current_ms = get_time_millis()
    elapsed_sec = float(current_ms - self._start_ms) / 1000.0
    if elapsed_sec > self._duration:
      return None
    return elapsed_sec

  def _create_image(self, color, alpha):
    if isinstance(color, basestring):
      color_rgb = ImageColor.getrgb(color)
    else:
      color_rgb = color
    return Image.new('RGBA', (self._width, self._height),
                     (color_rgb[0], color_rgb[1], color_rgb[2], alpha))

  def get_image(self, elapsed_sec):
    raise Exception('Effect.get_image() needs to be implemented by a subclass')


def register(name, cls):
  global _EFFECTS
  _EFFECTS[name] = cls


def load(name, w, h, **kwargs):
  effect = _EFFECTS[name](**kwargs)
  effect._set_dimensions(w, h)
  effect._prepare()
  return effect

