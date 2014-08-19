# -*- coding: utf-8 -*-
# Licensed under The MIT License
from .util import PROJECT_DIR, PACKAGE_DIR, VENV_DIR

FPS = 15

CLIPS_DIR = VENV_DIR + '/clips'
PLAYLISTS_DIR = VENV_DIR + '/playlists'

_SCREEN_FRAME_WIDTH = 500
IMAGE_FRAME_WIDTH = _SCREEN_FRAME_WIDTH / 2
FRAME_HEIGHT = 50
MESH_RATIO = 5  # Make it 50x10
TCL_CONTROLLER = 1
_USE_CC_TCL = True

class Player(object):

    def __init__(self,*args, **kwargs):
        self.status = "mock status"
        self.clip_name = "mock clip"

    def is_fin_enabled(self):
        return False
            
    def disable_reset(self):
        pass

    def __str__(self):
        return "mock player"

    def get_status_lines(self):
        return ["mock status 1", "mock status 2", "mock status 3"]

    def get_frame_size(self):
        return (_SCREEN_FRAME_WIDTH, FRAME_HEIGHT)

    def gamma_up(self):
        pass

    def gamma_down(self):
        pass

    def visualization_volume_up(self):
        pass

    def visualization_volume_down(self):
        pass

    @property
    def elapsed_time(self):
        return 0

    @property
    def volume(self):
        return 0

    @volume.setter
    def volume(self, value):
        pass

    def volume_up(self):
        pass

    def volume_down(self):
        pass

    def play(self, clip_idx):
        pass

    def toggle(self):
        pass

    def pause(self):
        pass

    def resume(self):
        pass

    def next(self):
        pass

    def prev(self):
        pass

    def skip_forward(self):
        pass

    def skip_backward(self):
        pass

    def skip(self, seconds):
        pass

    def toggle_visualization(self):
        pass

    def select_next_preset(self, is_forward):
        pass

    def toggle_hdr_mode(self):
        pass

    def get_frame_images(self, need_original, need_intermediate):
        return None

    def play_effect(self, name, **kwargs):
        pass

    def stop_effect(self):
        pass

    def run(self):
        pass