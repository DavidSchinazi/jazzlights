# -*- coding: utf-8 -*-

import logging
import os
import sys
import time

from .util import get_time_millis

_EFFECTS = {}


class FixedDurationEffect(object):

  def __init__(self, duration=None):
    self._duration = duration
    self._start_ms = get_time_millis()

  def __call__(self, frame, current_ms):
    elapsed_sec = float(current_ms - self._start_ms) / 1000.0
    if elapsed_sec > self._duration:
      return False
    self._apply(frame, elapsed_sec)
    return True


def register(name, cls):
  global _EFFECTS
  _EFFECTS[name] = cls


def load(name, **kwargs):
  return _EFFECTS[name](**kwargs)

