# -*- coding: utf-8 -*-
import PIL
import time
import logging

from PIL import Image, ImageFont, ImageDraw, ImageChops

from ..effect import FixedDurationEffect, register

class TestStripes(FixedDurationEffect):

    def __init__(self,duration=15,fgcolor="white", bgcolor="black", thickness=4):
        FixedDurationEffect.__init__(self,duration)
        self.fgcolor = fgcolor
        self.bgcolor = bgcolor
        self.thickness = thickness

    def _apply(self,frame,elapsed):
        (w,h) = frame.size
        frame.paste(self.bgcolor,(0,0,w,h))
        if elapsed < self.duration/2:
            x = int(2*w*elapsed/self.duration)
            frame.paste(self.fgcolor,(x,0, x+self.thickness,h))
        else:
            y = int(2*h*(elapsed - self.duration/2)/self.duration)
            frame.paste(self.fgcolor,(0,y,w,y+self.thickness))

register('teststripes', TestStripes)