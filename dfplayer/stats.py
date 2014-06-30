# -*- coding: utf-8 -*-

import collections
import math

from .util import get_time_millis

class Stats(object):

  def __init__(self, max_items):
    self._max_items = max_items
    self.reset()

  def reset(self):
    self._values = collections.deque(maxlen=self._max_items)
    self._times = collections.deque(maxlen=self._max_items)

  def add(self, v):
    self._values.append(float(v))
    self._times.append(get_time_millis())

  def get_earliest_time(self):
    if len(self._times) == 0:
      return get_time_millis()
    return self._times[0]

  def get_count(self):
    return len(self._values)

  def get_average(self):
    # TODO(igorc): Try to use numpy.
    length = len(self._values)
    if length == 0:
      return 0
    total = 0.0
    for v in self._values:
      total += v
    return total / length

  def get_average_and_stddev(self):
    length = len(self._values)
    if length == 0:
      return (0, 0)
    squares = 0.0
    avg = self.get_average()
    for v in self._values:
      d = v - avg
      squares += d * d
    return (avg, math.sqrt(squares / length))

