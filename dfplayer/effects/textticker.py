# -*- coding: utf-8 -*-
import PIL
import time
import logging

from PIL import Image, ImageFont, ImageDraw, ImageChops

from ..util import PROJECT_DIR
from ..player import FRAME_WIDTH,FRAME_HEIGHT
from ..effect import FixedDurationEffect, register

class TextTicker(FixedDurationEffect):

    def __init__(self,text="HELLO",duration=5,height=FRAME_HEIGHT,color="white"):
        FixedDurationEffect.__init__(self,duration)
        font = ImageFont.truetype( PROJECT_DIR + "/dfplayer/effects/impact.ttf", 
            int(height*0.8))
        (w,h) = font.getsize(text)
        self.rendered_text = Image.new("RGB", (int(w*1.6),height), "black")
        draw = ImageDraw.Draw(self.rendered_text)
        draw.text((int(w*0.3), int(height*0.1)),text,color,font=font)

    def _apply(self,frame,elapsed):
        fw = frame.size[0]
        tw = self.rendered_text.size[0]
        frame.paste( self.rendered_text, 
            (int((tw+fw)*elapsed/self.duration - tw),0))


register('textticker', TextTicker)