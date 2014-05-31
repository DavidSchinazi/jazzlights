# -*- coding: utf-8 -*-
from ..effect import FixedDurationEffect, register
from ..util import hsl2rgb

class Chameleon(FixedDurationEffect):

    def __init__(self,duration=5):
        FixedDurationEffect.__init__(self,duration)

    def _apply(self,frame,elapsed):
        (w,h) = frame.size
        hsl = (360*elapsed/self.duration,1,0.5)
        rgb = hsl2rgb(*hsl)
        frame.paste( rgb,(0,0,w,h))


register('chameleon', Chameleon)