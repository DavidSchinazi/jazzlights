# -*- coding: utf-8 -*-

from ..effect import FixedDurationEffect, register

class Blink(FixedDurationEffect):

    def __init__(self,duration=2,fgcolor="orange", bgcolor="black", freq=8):
        FixedDurationEffect.__init__(self,duration)
        self.fgcolor = fgcolor
        self.bgcolor = bgcolor
        self.freq = freq

    def _apply(self,frame,elapsed):
        (w,h) = frame.size
        phase = int(elapsed*self.freq) % 2
        if phase:
            frame.paste(self.fgcolor,(0,0,w,h))
        else:
            frame.paste(self.bgcolor,(0,0,w,h))


register('blink', Blink)