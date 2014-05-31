# -*- coding: utf-8 -*-
import os
import sys
import PIL
import logging

from gevent import sleep
from Tkinter import Tk, Frame, Canvas
from PIL import Image, ImageTk

from ..util import PROJECT_DIR

WIN_WIDTH = 1440
WIN_HEIGHT = 900
FPS = 30


class PlayerApp(Frame):

    def __init__(self, root, player):
        Frame.__init__(self, root)
        self.root = root
        self.pack()
        canvas = Canvas(root, width=WIN_WIDTH, height=WIN_HEIGHT,
                        borderwidth=0, background='black')
        canvas.pack()
        
        led_mask = Image.open(PROJECT_DIR + '/dfplayer/ledmask.png')
        img_size = (WIN_WIDTH, 
            int(led_mask.size[1] * float(WIN_WIDTH / led_mask.size[0])))
        self.led_mask = led_mask.resize(img_size, Image.NEAREST).convert("1")
        self.img = ImageTk.PhotoImage('RGB', img_size)
        canvas.create_image(WIN_WIDTH / 2, WIN_HEIGHT / 2, image=self.img)

        self.player = player
        
        self.root.bind('0', lambda e: player.stop_effect())
        self.root.bind('1', lambda e: player.play_effect('textticker'))
        self.root.bind('2', lambda e: player.play_effect('solidcolor'))
        self.root.bind('3', lambda e: player.play_effect('randompixels'))
        self.root.bind('4', lambda e: player.play_effect('teststripes'))
        self.root.bind('5', lambda e: player.play_effect('blink'))
        self.root.bind('6', lambda e: player.play_effect('chameleon'))
        self.root.bind('<space>', lambda e: player.toggle())
        self.root.bind('<Left>', lambda e: player.prev())
        self.root.bind('<Right>', lambda e: player.next())
        self.root.protocol('WM_DELETE_WINDOW', lambda : self.quit())
        self.root.after(1000 / FPS, lambda : self.update())
        self.running = True

    def run_effect(self,effect):
        self.player.effect = effect    

    def quit(self):
        self.running = False

    def update(self):
        try:
            img = self.player.frame()
            if img:
                img = img.resize(self.led_mask.size)
                img.paste("#000000", (0,0,self.led_mask.size[0], 
                    self.led_mask.size[1]), self.led_mask)
                self.img.paste(img)
            self.root.after(1000 / FPS, lambda : self.update())
        except KeyboardInterrupt:
            self.quit()


def run(player):
    root = Tk()
    root.geometry('%dx%d' % (WIN_WIDTH, WIN_HEIGHT))
    # root.attributes("-topmost", True)

    app = PlayerApp(root, player)
    
    # root.mainloop()
    while app.running:
        root.update()
        sleep(1.0 / FPS)


