# -*- coding: utf-8 -*-

import atexit
import gevent
import logging
import os
import PIL
import re
import StringIO
import subprocess
import sys
import time

from PIL import Image
from PIL import ImageChops
from mpd import MPDClient
from gevent import sleep
from gevent.coros import RLock

from .util import catch_and_log, PROJECT_DIR, PACKAGE_DIR, VENV_DIR
from .effect import load as load_effect
from .tcl_renderer import TclRenderer

FPS = 16
FRAME_WIDTH = 500
FRAME_HEIGHT = 50
TCL_CONTROLLER = 1

MPD_PORT = 6601
MPD_DIR = VENV_DIR + '/mpd/'
MPD_CONFIG_FILE = MPD_DIR + '/mpd.conf'
MPD_DB_FILE = MPD_DIR + '/tag_cache'
MPD_PID_FILE = MPD_DIR + '/mpd.pid'
MPD_LOG_FILE = MPD_DIR + '/mpd.log'
MPD_CARD_ID = -1  # From 'aplay -l'

CLIPS_DIR = VENV_DIR + '/clips/'
PLAYLISTS_DIR = VENV_DIR + '/playlists/'

MPD_CONFIG_TPL = '''
music_directory     "%(CLIPS_DIR)s"
playlist_directory  "%(PLAYLISTS_DIR)s"
db_file             "%(MPD_DB_FILE)s"
pid_file            "%(MPD_PID_FILE)s"
log_file            "%(MPD_LOG_FILE)s"
port                "%(MPD_PORT)d"
audio_output {
    type            "alsa"
    name            "USB DAC"
    device          "hw:%(MPD_CARD_ID)s,0"
    auto_resample   "no"
    mixer_control   "PCM"
    mixer_type      "hardware"
    mixer_device    "hw:%(MPD_CARD_ID)s"
}
'''


class Player(object):

    def __init__(self):
        self._start_mpd()
        self.lock = RLock()
        with self.lock:
            self.mpd = MPDClient()
            self.mpd.connect('localhost', MPD_PORT)

        self._fetch_playlist()
        self._fetch_state()

        self._effect = None
        self._frame = None
        self._prev_frame_file = ''

        self._tcl = TclRenderer(TCL_CONTROLLER)
        self._tcl.set_dimensions(FRAME_WIDTH, FRAME_HEIGHT)

    def __str__(self):
        elapsed = int(self.elapsed_time)
        return 'Player [%s %s %02d:%02d]' % (self.status,
                self.clip_name, elapsed / 60, elapsed % 60)

    def _fetch_playlist(self):
        self.playlist = []
        self.songid_to_idx = {}
        with self.lock:
            listinfo = self.mpd.playlistinfo()
        # TODO(azov): Can we check for completion in a loop with shorter sleep?
        sleep(0.5)  # let it actually load the playlist
        for song in listinfo:
            self.songid_to_idx[song['id']] = len(self.playlist)
            self.playlist.append((song['file'])[0:-4])

    def _fetch_state(self):
        # TODO(igorc): Why do we lock access to mpd, but not the rest of vars?
        # TODO(igorc): Add "mpd" prefix to all mpd-related state vars.
        with self.lock:
            s = self.mpd.status()
        self._mpd_state_ts = time.time()
        self._volume = 0.01 * float(s['volume'])
        if s['state'] == 'play':
            self.status = 'playing'
        elif s['state'] == 'pause':
            self.status = 'paused'
        else:
            self.status = 'idle'
            self.clip_name = None
            self._mpd_elapsed_time = 0

        if 'error' in s:
            # TODO(azov): Report error on the UI.
            logging.info('MPD Error = \'%s\'', s['error'])

        if self.status != 'idle':
            self.clip_name = self.playlist[self.songid_to_idx[s['songid']]]
            self._mpd_elapsed_time = float(s['elapsed'])

        # print 'MPD status', s

    def _config_mpd(self):
        for d in (MPD_DIR, CLIPS_DIR, PLAYLISTS_DIR):
            if not os.path.exists(d):
                os.makedirs(d)

        self._update_card_id()

        with open(MPD_CONFIG_FILE, 'w') as out:
            out.write(MPD_CONFIG_TPL % globals())

    def _update_card_id(self):
        global MPD_CARD_ID
        if MPD_CARD_ID != -1:
            return

        with open('/proc/asound/modules') as f:
            alsa_modules = f.readlines()
        re_term = re.compile("\s*(\d*)\s*snd_usb_audio\s*")
        for line in alsa_modules:
            m = re_term.match(line)
            if m:
                MPD_CARD_ID = int(m.group(1))
                logging.info('Located USB card #%s' % MPD_CARD_ID)
                break
        if MPD_CARD_ID == -1:
            logging.info('Unable to find USB card id')
            MPD_CARD_ID = 1  # Maybe better than nothing

    def _stop_mpd(self):
        self._config_mpd()
        if os.path.exists(MPD_PID_FILE):
            logging.info('Stopping mpd')
            subprocess.call(['mpd', '--kill', MPD_CONFIG_FILE])

    def _start_mpd(self):
        self._config_mpd()
        logging.info('Starting mpd')

        if os.path.exists(MPD_DB_FILE):
            os.unlink(MPD_DB_FILE)

        subprocess.check_call(['mpd', MPD_CONFIG_FILE])
        atexit.register(lambda : self._stop_mpd())

    @property
    def elapsed_time(self):
        if self.status == 'playing':
            return time.time() - self._mpd_state_ts + self._mpd_elapsed_time
        else:
            return self._mpd_elapsed_time

    @property
    def volume(self):
        return self._volume

    @volume.setter
    def volume(self, value):
        if value < 0:
            value = 0
        elif value > 1:
            value = 1
        logging.info('Setting volume to %s', value)
        with self.lock:
            self.mpd.setvol(int(float(value) * 100))
            self._volume = value  # Till next status sync.

    def volume_up(self):
        self.volume = self.volume + 0.05

    def volume_down(self):
        self.volume = self.volume - 0.05

    def load_playlist(self, name):
        with self.lock:
            self.mpd.clear()
            self.mpd.load(name)
            self.mpd.repeat(1)
            self.mpd.single(1)
        self._fetch_playlist()
        self._fetch_state()
        logging.info("Loaded playlist '%s': %s" % (name, self.playlist))

    def play(self, clip_idx):
        with self.lock:
            if len(self.playlist) < 1:
                logging.error('Playlist is empty')
            else:
                self.mpd.play(clip_idx % len(self.playlist))

    def toggle(self):
        if self.status == 'idle':
            self.play(0)
        elif self.status == 'playing':
            self.pause()
        else:
            self.resume()

    def pause(self):
        with self.lock:
            self.mpd.pause(1)

    def resume(self):
        with self.lock:
            self.mpd.pause(0)

    def next(self):
        with self.lock:
            self.mpd.next()

    def prev(self):
        with self.lock:
            self.mpd.previous()

    def get_frame_image(self):
        if self.status == 'idle':
            # TODO(igorc): Keep drawing some neutral pattern for fun.
            return None
        else:
            # TODO(igorc): Clarify frame sync. The caller calls this function
            # at 30 FPS, while we generate at 16 FPS max. This may cause some
            # uneven frame rate. Basically, there is no guarantee that the
            # caller's timing  will be well-aligned with elapsed frame counter.
            frame_num = 1 + int(self.elapsed_time * FPS)
            frame_file = CLIPS_DIR + '%s/frame%05d.jpg' \
                % (self.clip_name, frame_num)
            if frame_file == self._prev_frame_file:
                return self._frame
            if not os.path.exists(frame_file):
                # TODO(igorc): Handle the case where '_frame' was neevr loaded.
                return self._frame
            with open(frame_file, 'rb') as imgfile:
                img = Image.open(imgfile)
                img.load()
                if self._effect is not None:
                    if not self._effect(img, self.elapsed_time):
                        self._effect = None
                self._frame = img
            self._prev_frame_file = frame_file
            self._tcl.send_frame(list(self._frame.getdata()))
            return self._frame

    def play_effect(self, name, **kwargs):
        logging.info("Playing %s: %s", name, kwargs)
        self._effect = load_effect(name, **kwargs)
    
    def stop_effect(self):
        self._effect = None    

    def run(self):
        while True:
            with catch_and_log():
                self._fetch_state()
                logging.info(self)
                # with self.lock:
                #    self.mpd.idle()
                sleep(1)


