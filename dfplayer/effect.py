# -*- coding: utf-8 -*-
import os
import sys
import time
import logging

effects = {}


class FixedDurationEffect(object):

    def __init__(self, duration=None):
        self.duration = duration
        self.start_ts = time.time()

    def __call__(self, frame, elapsed):
        elapsed = time.time() - self.start_ts # ignore clip time
        if self.duration < elapsed:
            return False
        else:
            self._apply(frame,elapsed)
            return True


def register(name, cls):
    global effects
    effects[name] = cls


def load(name, *args, **kwargs):
    return effects[name](*args, **kwargs)
