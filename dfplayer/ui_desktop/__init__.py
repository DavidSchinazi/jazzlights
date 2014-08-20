# -*- coding: utf-8 -*-
# Licensed under The MIT License

import io
import logging
import os
import PIL
import sys

from gevent import sleep
from Tkinter import Tk, Frame, Canvas
from PIL import Image, ImageTk

from ..util import PROJECT_DIR

WIN_WIDTH = 1224
WIN_HEIGHT = int(WIN_WIDTH / 1.333)

FPS = 15


# TODO(igorc): It would be nice to self-resize, but the window keeps growing.
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
        self.config(bd=0, bg='black', highlightbackground='black')
        self.pack()
        self._canvas = Canvas(
            root, width=(WIN_WIDTH), height=WIN_HEIGHT,
            bd=0, bg='black', highlightbackground='black')
        self._canvas.pack()

        self.player = player

        self._img_mode = 4
        self._img_mode_count = 6

        # Scale the size to with the entire width of our window.
        # TODO(igorc): Avoid scaling? Or auto-rescale when window resizes.
        frame1_size = self.player.get_frame_size()
        img1_aspect = frame1_size[0] / 2 / frame1_size[1]
        img1_width = int(WIN_WIDTH * 0.95)
        self._img1_size = (img1_width, int(img1_width / img1_aspect))

        if self.player.is_fin_enabled():
            text_y = 0.09
            img1_y = 0.28
            img2_y = 0.56
        else:
            text_y = 0.06
            img1_y = 0.23
            img2_y = 0.54

        # Undo gamma correction in LED preview, otherwise it is too dark.
        # Keep a fixed value to have better feedback on LED gamma changes.
        left_x = int(WIN_WIDTH * 0.025)
        self._img1 = ImageTk.PhotoImage('RGBA', self._img1_size, gamma=2.2)
        self._img2 = ImageTk.PhotoImage('RGBA', self._img1_size)
        self._canvas.create_image(
            left_x, int(WIN_HEIGHT * img1_y),
            anchor='nw', image=self._img1)
        self._canvas.create_image(
            left_x, int(WIN_HEIGHT * img2_y),
            anchor='nw', image=self._img2)

        if self.player.is_fin_enabled():
            img3_width = int(WIN_WIDTH * 0.15)
            # The fin dimensions are 150x240 inches.
            self._img3_size = (img3_width, int(img3_width * 1.1))
            self._img3 = ImageTk.PhotoImage('RGBA', self._img3_size, gamma=2.2)
            self._canvas.create_image(
                int(WIN_WIDTH * 0.65), int(WIN_HEIGHT * 0.05),
                anchor='nw', image=self._img3)

        self._main_label = self._canvas.create_text(
            (left_x, WIN_HEIGHT * text_y),
            fill='#fff', anchor='nw', justify='left',
            font=("Sans Serif", 16))
        self._canvas.itemconfig(self._main_label, text="this is the text")

        self.root.bind('q', lambda e: player.volume_up())
        self.root.bind('a', lambda e: player.volume_down())
        self.root.bind('m', lambda e: self.switch_mask())
        self.root.bind('g', lambda e: player.gamma_up())
        self.root.bind('b', lambda e: player.gamma_down())
        self.root.bind('h', lambda e: player.visualization_volume_up())
        self.root.bind('n', lambda e: player.visualization_volume_down())
        self.root.bind('v', lambda e: player.toggle_visualization())
        self.root.bind('t', lambda e: self.toggle_visualizer_render())
        self.root.bind('d', lambda e: player.toggle_hdr_mode())
        self.root.bind('o', lambda e: player.select_next_preset(False))
        self.root.bind('p', lambda e: player.select_next_preset(True))
        self.root.bind('0', lambda e: player.stop_effect())
        self.root.bind('1', lambda e: player.play_effect('textticker'))
        self.root.bind('2', lambda e: player.play_effect('solidcolor'))
        self.root.bind('3', lambda e: player.play_effect('randompixels'))
        self.root.bind('4', lambda e: player.play_effect('teststripes'))
        self.root.bind('5', lambda e: player.play_effect('blink'))
        self.root.bind('6', lambda e: player.play_effect('chameleon'))
        self.root.bind('7', lambda e: player.play_effect('indicator'))
        self.root.bind('8', lambda e: player.play_effect('flick'))
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
        self._img_mode += 1
        self._img_mode = self._img_mode % self._img_mode_count

    def toggle_visualizer_render(self):
        if (self._img_mode % 3) == 0:
            self._img_mode += 1
        elif (self._img_mode % 3) == 2:
            self._img_mode += 2
        else:
            self._img_mode += 2
        self._img_mode = self._img_mode % self._img_mode_count

    def quit(self):
        self.running = False

    def _paste_images(self, images):
        # Always use LED image as the first image.
        frame1 = images[2].copy() if images[2] else None
        frame3 = images[3].copy() if images[3] else None

        crop_rect = None
        frame_size = self.player.get_frame_size()
        if self._img_mode < 3:
            crop_rect = (0, 0, frame_size[0] / 2, frame_size[1])
        else:
            crop_rect = (frame_size[0] / 2, 0, frame_size[0], frame_size[1])
        if frame1:
            frame1 = frame1.crop(crop_rect)

        if (self._img_mode % 3) == 0 and not self.player.is_playing_effect():
            # Second is intermediate image by default.
            frame2 = images[1].copy() if images[1] else None
            if frame2:
                frame2 = frame2.crop(crop_rect)
        elif (self._img_mode % 3) == 1 and not self.player.is_playing_effect():
            # Otherwise show original image.
            frame2 = images[0].copy() if images[0] else None
            if frame2 and self._img_mode >= 3:
                frame2 = frame2.transpose(Image.FLIP_LEFT_RIGHT)
        else:
            # Show black if disabled.
            frame2 = Image.new('RGBA', self._img1_size, (0, 0, 0, 0))

        if frame1:
            self._img1.paste(frame1.resize(self._img1_size))
        if frame2:
            self._img2.paste(frame2.resize(self._img1_size))
        if frame3:
            self._img3.paste(frame3.resize(self._img3_size))

    def update(self):
        try:
            need_original = (self._img_mode % 3) == 1
            need_intermediate = (self._img_mode % 3) == 0
            images = self.player.get_frame_images(
                need_original, need_intermediate)
            if images:
                self._paste_images(images)
            status_lines = self.player.get_status_lines()
            self._canvas.itemconfig(
                self._main_label, text='\n'.join(status_lines))
            # TODO(igorc): Reschedule after errors.
            self.root.after(1000 / FPS / 4, lambda : self.update())
        except KeyboardInterrupt:
            self.quit()


def run(player, args):
    root = Tk()
    # width, height = (root.winfo_screenwidth(), root.winfo_screenheight())
    if args.max:
        root.attributes('-fullscreen', True)
    else:
        root.geometry('%dx%d' % (WIN_WIDTH, WIN_HEIGHT))
    root.configure(bg='black', bd=0, highlightbackground='black')
    # root.attributes("-topmost", True)

    app = PlayerApp(root, player)
    
    # root.mainloop()
    while app.running:
        root.update()
        sleep(1.0 / FPS)

