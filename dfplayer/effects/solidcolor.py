# -*- coding: utf-8 -*-
import PIL
import time
import logging

from PIL import Image, ImageFont, ImageDraw, ImageChops

from ..effect import FixedDurationEffect, register

class SolidColor(FixedDurationEffect):

    def __init__(self,duration=2,color="green"):
        FixedDurationEffect.__init__(self,duration)
        self.color = color

    def _apply(self,frame,elapsed):
        frame.paste(self.color,(0, 0, frame.size[0], frame.size[1]))


register('solidcolor', SolidColor)