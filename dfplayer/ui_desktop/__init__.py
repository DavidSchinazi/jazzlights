# -*- coding: utf-8 -*-

import io
import logging
import os
import PIL
import sys

from gevent import sleep
from Tkinter import Tk, Frame, Canvas
from PIL import Image, ImageTk

from ..util import PROJECT_DIR

WIN_WIDTH = 1024
WIN_HEIGHT = 768
FPS = 30


# TODO(igorc): It would be nice to self-resize, buyt the window keeps growing.
# http://stackoverflow.com/questions/22838255
class ResizingCanvas(Canvas):

    def __init__(self, parent, **kwargs):
        Canvas.__init__(self, parent, **kwargs)
        self.bind("<Configure>", self.on_resize)

    def on_resize(self, event):
        self.width = event.width
        self.height = event.height
        self.config(width=self.width, height=self.height)


class PlayerApp(Frame):

    def __init__(self, root, player):
        Frame.__init__(self, root)
        self.root = root
        self.pack()
        self._canvas = Canvas(
            root, width=WIN_WIDTH, height=WIN_HEIGHT,
            borderwidth=0, background='black')
        self._canvas.pack()

        self._main_label = self._canvas.create_text(
            WIN_WIDTH / 2, WIN_HEIGHT / 20, fill='#fff')
        self._canvas.itemconfig(self._main_label, text="this is the text")

        self.player = player

        # Scale the size to with the entire width of our window.
        # TODO(igorc): Avoid scaling? Or auto-rescale when window resizes.
        img_size = self.player.get_frame_size()
        img_size = (WIN_WIDTH,
            int(round((float(WIN_WIDTH) / img_size[0]) * img_size[1])))

        self._led_mask1 = self.create_led_mask(200)
        self._led_mask2 = self.create_led_mask(255)
        self._led_mask1 = self._led_mask1.resize(img_size, Image.NEAREST)
        self._led_mask2 = self._led_mask2.resize(img_size, Image.NEAREST)
        self._led_mask = self._led_mask1

        self._img = ImageTk.PhotoImage('RGB', img_size)
        self._canvas.create_image(
            WIN_WIDTH / 2, WIN_HEIGHT / 2, image=self._img)

        self.root.bind('q', lambda e: player.volume_up())
        self.root.bind('a', lambda e: player.volume_down())
        self.root.bind('m', lambda e: self.switch_mask())
        self.root.bind('s', lambda e: player.toggle_split_sides())
        self.root.bind('g', lambda e: player.gamma_up())
        self.root.bind('b', lambda e: player.gamma_down())
        self.root.bind('0', lambda e: player.stop_effect())
        self.root.bind('1', lambda e: player.play_effect('textticker'))
        self.root.bind('2', lambda e: player.play_effect('solidcolor'))
        self.root.bind('3', lambda e: player.play_effect('randompixels'))
        self.root.bind('4', lambda e: player.play_effect('teststripes'))
        self.root.bind('5', lambda e: player.play_effect('blink'))
        self.root.bind('6', lambda e: player.play_effect('chameleon'))
        self.root.bind('<space>', lambda e: player.toggle())
        self.root.bind('<Up>', lambda e: player.prev())
        self.root.bind('<Down>', lambda e: player.next())
        self.root.bind('<Left>', lambda e: player.skip_backward())
        self.root.bind('<Right>', lambda e: player.skip_forward())
        self.root.protocol('WM_DELETE_WINDOW', lambda : self.quit())
        self.root.after(1000 / FPS / 4, lambda : self.update())
        self.running = True

    def run_effect(self, effect):
        self.player.effect = effect    

    def switch_mask(self):
        if self._led_mask == self._led_mask1:
            self._led_mask = self._led_mask2
        else:
            self._led_mask = self._led_mask1

    def quit(self):
        self.running = False

    def create_led_mask(self, default_alpha):
        # return Image.open(PROJECT_DIR + '/dfplayer/ledmask.png')
        size = self.player.get_frame_size()
        img_bytes = [default_alpha] * size[0] * size[1] * 4
        for c in self.player.get_tcl_coords():
            x, y = c
            idx = (x + y * size[0]) * 4
            img_bytes[idx + 3] = 0
        img_bytes = str(bytearray(img_bytes))
        return Image.frombytes('RGBA', size, img_bytes)

    def update(self):
        try:
            frame = self.player.get_frame_image()
            if frame:
                frame = frame.resize(self._led_mask.size)
                frame.paste("#000000",
                    (0, 0, self._led_mask.size[0], self._led_mask.size[1]),
                    self._led_mask)
                self._img.paste(frame)
            self._canvas.itemconfig(
                self._main_label, text=self.player.get_status_str())
            self.root.after(1000 / FPS / 4, lambda : self.update())
        except KeyboardInterrupt:
            self.quit()


def run(player):
    root = Tk()
    # width, height = (root.winfo_screenwidth(), root.winfo_screenheight())
    root.geometry('%dx%d' % (WIN_WIDTH, WIN_HEIGHT))
    # root.attributes("-topmost", True)

    app = PlayerApp(root, player)
    
    # root.mainloop()
    while app.running:
        root.update()
        sleep(1.0 / FPS)


