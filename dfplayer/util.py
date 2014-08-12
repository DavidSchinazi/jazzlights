# -*- coding: utf-8 -*-
# Licensed under The MIT License

import contextlib
import functools
import logging
import os
import pdb
import time
import traceback

from PIL import Image
from PIL import ImageColor

PACKAGE_DIR = os.path.abspath(os.path.dirname(__file__))
PROJECT_DIR = os.path.dirname(PACKAGE_DIR)
VENV_DIR = os.path.join(PROJECT_DIR, 'env')


def get_time_millis():
    return int(round(time.time() * 1000))


def create_image(w, h, color, alpha):
  if isinstance(color, basestring):
    rgb = ImageColor.getrgb(color)
  else:
    rgb = color
  return Image.new('RGBA', (w, h), (rgb[0], rgb[1], rgb[2], alpha))


def hsl2rgb(H, S, L):
    if L < 0.5:
        q = L * (1 + S)
    elif L >= 0.5:
        q = L + S - (L * S)
    p = 2 * L - q
    hk = H / 360.0
    tR = hk + 0.333333
    tG = hk
    tB = hk - 0.333333

    def color(t):
        if t < 0:
            t += 1
        if t > 1:
            t -= 1
        if t < 0.166666:
            c = p + ((q - p) * 6 * t)
        elif 0.166666 <= t < 0.5:
            c = q
        elif 0.5 <= t < 0.666667:
            c = p + ((q - p) * 6 * (0.666667 - t))
        else:
            c = p
        return c

    R = int(color(tR) * 255)
    G = int(color(tG) * 255)
    B = int(color(tB) * 255)
    return R, G, B


class ExceptionLogger(object):

    def __init__(self, level=logging.ERROR, catch=Exception,
        ignore=(KeyboardInterrupt, SystemExit)):
        self.level = level
        self.catch = catch
        self.ignore = ignore

    def __call__(self, func):

        @functools.wraps(func)
        def decorated(*args, **kwargs):
            with self:
                return func(*args, **kwargs)
        return decorated

    def __enter__(self):
        pass

    def __exit__(self, errtype, errvalue, errtrace):
        if errvalue is not None:
            if not isinstance(errvalue, self.catch) \
                or isinstance(errvalue, self.ignore):
                return False
            else:
                logging.log(self.level, errtype.__name__ + ': '
                            + ''.join(traceback.format_exception(errtype,
                            errvalue, errtrace)))
                return True


def catch_and_log(*args, **kwargs):
    return ExceptionLogger(*args, **kwargs)


