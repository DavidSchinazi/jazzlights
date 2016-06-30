# -*- coding: utf-8 -*-
import ctypes
import logging
import os
import sys
import time


from ctypes import cdll, c_char_p
from Tkinter import Tk, Frame, Canvas
#from PIL import Image, ImageTk

WIN_WIDTH = 800
WIN_HEIGHT = 600

class DemoApp(Frame):

    def __init__(self, root):
        Frame.__init__(self, root)
        self.root = root
        #self.config(bd=0, bg='black', highlightbackground='black')
        self.pack()
        self._canvas = Canvas(root, width=WIN_WIDTH, height=WIN_HEIGHT, 
            bd=0, bg='black', highlightbackground='black')
        #self._canvas = Canvas(
        #    root, width=(WIN_WIDTH), height=WIN_HEIGHT,
        #    bd=0, bg='black', highlightbackground='black')
        self._canvas.create_oval((5,5,70,70), fill='red');
        self._canvas.pack()

        #self._canvas.create_image(
        #    0, int(WIN_HEIGHT * img2_y),
        #    anchor='nw', image=self._img2)

        # self.root.bind('7', lambda e: player.play_effect('indicator'))
        # self.root.bind('8', lambda e: player.play_effect('flick'))
        # self.root.bind('<space>', lambda e: player.toggle())
        # self.root.bind('<Up>', lambda e: player.prev())
        # self.root.bind('<Down>', lambda e: player.next())
        # self.root.bind('<Left>', lambda e: player.skip_backward())
        # self.root.bind('<Right>', lambda e: player.skip_forward())
        self.root.protocol('WM_DELETE_WINDOW', lambda : self.quit())
        #self.root.after(1000 / FPS / 4, lambda : self.update())
        self.running = True

    def quit(self):
        self.running = False

    def update(self):
        try:
            pass
            # self.root.after(1000 / FPS / 4, lambda : self.update())
        except KeyboardInterrupt:
            self.quit()


dfsparks = ctypes.cdll.LoadLibrary('libdfsparks2.dylib')
dfsparks.proto_id.restype = c_char_p
print dfsparks.proto_id()
exit(0)

root = Tk()
# width, height = (root.winfo_screenwidth(), root.winfo_screenheight())
root.geometry('%dx%d' % (WIN_WIDTH, WIN_HEIGHT))
root.configure(bg='black', bd=0, highlightbackground='black')
# root.attributes("-topmost", True)

app = DemoApp(root)

while app.running:
    root.update()
    time.sleep(1.0/60)


