import os
import sys
import PIL
import time
import gevent
import logging
import StringIO

from PIL import Image
from PIL import ImageChops
from mpd import MPDClient
from gevent import sleep
from gevent.coros import RLock
 
from .util import ExceptionLogger, PROJECT_DIR, PACKAGE_DIR

FPS = 16

class Player(object):
    def __init__(self):
        self.lock = RLock()
        with self.lock:
            self.mpd = MPDClient()               
            self.mpd.connect("localhost", 6600)

        self._fetch_playlist()
        self._fetch_state()

        self._frame = None
        self.led_mask = Image.open( PROJECT_DIR + 
            "/dfplayer/ui_web/static/images/dfleds.png")

    def __str__(self):
        elapsed = int(self.elapsed)
        return "Player [%s %s %02d:%02d]" % (self.status, self.clip_name,
            elapsed/60, elapsed%60)

    def _fetch_playlist(self):
        self.playlist = []
        self.songid_to_idx = {}
        with self.lock:
            listinfo = self.mpd.playlistinfo()
        for song in listinfo:
            self.songid_to_idx[song['id']] = len(self.playlist)
            self.playlist.append( song['file'][0:-4] )

    def _fetch_state(self):
        with self.lock:
            s = self.mpd.status()
        self._state_ts = time.time()
        self.status = 'playing' if s['state'] == 'play' else 'paused'
        self._volume = 0.01*float(s['volume'])
        if 'songid' in s:
            self.clip_name = self.playlist[self.songid_to_idx[s['songid']]]
        if 'elapsed' in s:    
            self._elapsed = float(s['elapsed'])

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
            self.mpd.setvol(float(value)*100)
            
    def reload_playlist(self):    
        with self.lock:
            self.mpd.clear()
            self.mpd.load("playlist")
            self.mpd.play(0)    
            self.mpd.pause(1)
            self.mpd.repeat(1)
            self.mpd.single(1)
        self._fetch_playlist()
        logging.info( "Reloaded playlist: %s" % self.playlist )

    def play(self, clip_idx):
        with self.lock:
            self.mpd.play(clip_idx % len(self.playlist))

    def pause(self):
        with self.lock:
            self.mpd.pause(1)    

    def resume(self):
        with self.lock:
            self.mpd.pause(0)    

    def next(self):
        with self.lock:
            self.mpd.next();

    def prev(self):
        with self.lock:
            self.mpd.previous();

    def frame(self):
        frame_num = 1 + int(self.elapsed * FPS)
        frame_file = PROJECT_DIR + "/env/clips/%s/frame%05d.jpg" % (
            self.clip_name, frame_num)
        if os.path.exists(frame_file):
            with open( frame_file, "rb") as imgfile:
                img = Image.open( imgfile )
                img.load()
                img = ImageChops.multiply(img, self.led_mask)
                self._frame = img
        return self._frame

    def run(self):
        while True:
            #with ExceptionLogger():
                self._fetch_state()
                logging.info(self)
                #with self.lock:
                #    self.mpd.idle()
                sleep(1)
