# -*- coding: utf-8 -*-
import os
import sys
import PIL
import time
import gevent
import atexit
import logging
import StringIO
import subprocess

from PIL import Image
from PIL import ImageChops
from mpd import MPDClient
from gevent import sleep
from gevent.coros import RLock

from util import catch_and_log, PROJECT_DIR, PACKAGE_DIR, VENV_DIR

FPS = 16
FRAME_WIDTH = 500
FRAME_HEIGHT = 50

MPD_PORT = 6601
MPD_DIR = VENV_DIR + '/mpd/'
MPD_CONFIG_FILE = MPD_DIR + '/mpd.conf'
MPD_DB_FILE = MPD_DIR + '/tag_cache'
MPD_PID_FILE = MPD_DIR + '/mpd.pid'
MPD_LOG_FILE = MPD_DIR + '/mpd.log'

CLIPS_DIR = VENV_DIR + '/clips/'
PLAYLISTS_DIR = VENV_DIR + '/playlists/'

MPD_CONFIG_TPL = '''
music_directory     "%(CLIPS_DIR)s"
playlist_directory  "%(PLAYLISTS_DIR)s"
db_file             "%(MPD_DB_FILE)s"
pid_file            "%(MPD_PID_FILE)s"
log_file            "%(MPD_LOG_FILE)s"
port                "%(MPD_PORT)d"
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

        self._frame = None
        self.led_mask = Image.open(PROJECT_DIR + '/dfplayer/ledmask.png')

    def __str__(self):
        elapsed = int(self.elapsed)
        return 'Player [%s %s %02d:%02d]' % (self.status,
                self.clip_name, elapsed / 60, elapsed % 60)

    def _fetch_playlist(self):
        self.playlist = []
        self.songid_to_idx = {}
        with self.lock:
            listinfo = self.mpd.playlistinfo()
        sleep(0.5)  # let it actually load the playlist
        for song in listinfo:
            self.songid_to_idx[song['id']] = len(self.playlist)
            self.playlist.append((song['file'])[0:-4])

    def _fetch_state(self):
        with self.lock:
            s = self.mpd.status()
        self._state_ts = time.time()
        self._volume = 0.01 * float(s['volume'])
        if s['state'] == 'play':
            self.status = 'playing'
        elif s['state'] == 'pause':
            self.status = 'paused'
        else:
            self.status = 'idle'
            self.clip_name = None
            self._elapsed = 0

        if self.status != 'idle':
            self.clip_name = self.playlist[self.songid_to_idx[s['songid']]]
            self._elapsed = float(s['elapsed'])

    def _config_mpd(self):
        for d in (MPD_DIR, CLIPS_DIR, PLAYLISTS_DIR):
            if not os.path.exists(d):
                os.makedirs(d)

        with open(MPD_CONFIG_FILE, 'w') as out:
            out.write(MPD_CONFIG_TPL % globals())

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
    def elapsed(self):
        if self.status == 'playing':
            return time.time() - self._state_ts + self._elapsed
        else:
            return self._elapsed

    @property
    def volume(self):
        return self._volume

    @volume.setter
    def volume(self, value):
        with self.lock:
            self.mpd.setvol(float(value) * 100)

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

    def frame(self):
        if self.status == 'idle':
            return None
        else:
            frame_num = 1 + int(self.elapsed * FPS)
            frame_file = CLIPS_DIR + '%s/frame%05d.jpg' \
                % (self.clip_name, frame_num)
            if os.path.exists(frame_file):
                with open(frame_file, 'rb') as imgfile:
                    img = Image.open(imgfile)
                    img.load()
                    img = ImageChops.multiply(img, self.led_mask)
                    self._frame = img
            return self._frame

    def run(self):
        while True:
            with catch_and_log():
                self._fetch_state()
                logging.info(self)
                # with self.lock:
                #    self.mpd.idle()
                sleep(1)


