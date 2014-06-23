# -*- coding: utf-8 -*-

import atexit
import gevent
import logging
import math
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
_SCREEN_FRAME_WIDTH = 500
IMAGE_FRAME_WIDTH = _SCREEN_FRAME_WIDTH / 2
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


def _get_time_millis():
    return int(round(time.time() * 1000))


# TODO(igorc): Move this into a different file
class Stats(object):

    def __init__(self):
        self.reset()

    def reset(self):
        self._values = []
        self._sum = 0

    def add(self, v):
        v = float(v)
        self._values.append(v)
        self._sum += v

    def get_average(self):
        if len(self._values) == 0:
            return 0
        return self._sum / len(self._values)

    def get_stddev(self):
        if len(self._values) == 0:
            return 0
        squares = 0
        avg = self.get_average()
        for v in self._values:
            d = v - avg
            squares += d * d
        return math.sqrt(squares / len(self._values))


class Player(object):

    def __init__(self):
        self._start_mpd()
        self.lock = RLock()
        with self.lock:
            self.mpd = MPDClient()
            self.mpd.connect('localhost', MPD_PORT)

        self._fetch_playlist()
        self._fetch_state()

        self._target_gamma = 2.4
        self._split_sides = False
        self._seek_time = None
        self._effect = None
        self._frame = None
        self._prev_frame_file = ''
        self._reset_stats()

        self._tcl = TclRenderer(TCL_CONTROLLER)
        self._tcl.set_layout(
            'dfplayer/layout.dxf', _SCREEN_FRAME_WIDTH, FRAME_HEIGHT)
        self._tcl.set_gamma(self._target_gamma)

    def __str__(self):
        elapsed_time = self.elapsed_time
        elapsed_sec = int(elapsed_time)
        if elapsed_time > 2:
            fps = int(float(self._frame_render_count) / elapsed_time)
        else:
            fps = FPS
        return ('Player [%s %s %02d:%02d] (fps=%s, skip=%s, delay=%s/%s, '
                'render=%d/%d)') % (
                   self.status, self.clip_name,
                   elapsed_sec / 60, elapsed_sec % 60,
                   fps, self._skipped_frame_count,
                   int(self._frame_delay_stats.get_average()),
                   int(self._frame_delay_stats.get_stddev()),
                   int(self._render_durations.get_average()),
                   int(self._render_durations.get_stddev()))

    def get_status_str(self):
        if self._seek_time:
            elapsed_time = self._seek_time
        else:
            elapsed_time = self.elapsed_time
        elapsed_sec = int(elapsed_time)
        return ('%s / %s / %02d:%02d') % (
            self.status.upper(), self.clip_name,
            elapsed_sec / 60, elapsed_sec % 60)

    def get_frame_size(self):
        return (_SCREEN_FRAME_WIDTH, FRAME_HEIGHT)

    def get_tcl_coords(self):
        return self._tcl.get_layout_coords()

    def toggle_split_sides(self):
        self._split_sides = not self._split_sides

    def gamma_up(self):
        self._target_gamma += 0.1
        print 'Setting gamma to %s' % self._target_gamma
        self._tcl.set_gamma(self._target_gamma)

    def gamma_down(self):
        self._target_gamma -= 0.1
        if self._target_gamma <= 0:
            self._target_gamma = 0.1
        print 'Setting gamma to %s' % self._target_gamma
        self._tcl.set_gamma(self._target_gamma)

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
        self._seek_time = None
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
            self._songid = s['songid']
            new_clip = self.playlist[self.songid_to_idx[self._songid]]
            if new_clip != self.clip_name:
                self._reset_stats()
            self.clip_name = new_clip
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

    def skip_forward(self):
        self.skip(20)

    def skip_backward(self):
        self.skip(-20)

    def skip(self, seconds):
        if self.status == 'idle':
            return
        if self._seek_time:
            self._seek_time = int(self._seek_time + seconds)
        else:
            self._seek_time = int(self._mpd_elapsed_time + seconds)
        with self.lock:
            self.mpd.seekid(self._songid, '%s' % self._seek_time)

    def get_frame_image(self):
        if self.status == 'idle':
            # TODO(igorc): Keep drawing some neutral pattern for fun.
            return None
        else:
            # TODO(igorc): Clarify frame sync. The caller calls this function
            # at 30 FPS, while we generate at 16 FPS max. This may cause some
            # uneven frame rate. Basically, there is no guarantee that the
            # caller's timing  will be well-aligned with elapsed frame counter.
            # TODO(igorc): Preload first 5 frames of every clip, and
            # keep the next 5 frames always preloaded too.
            elapsed_time = self.elapsed_time
            frame_num = int(elapsed_time * FPS)
            frame_file = CLIPS_DIR + '%s/frame%06d.jpg' \
                % (self.clip_name, frame_num + 1)
            if frame_file == self._prev_frame_file:
                return self._frame

            if not os.path.exists(frame_file):
                # TODO(igorc): Handle the case where '_frame' was neevr loaded.
                return self._frame

            start_time = time.time()
            with open(frame_file, 'rb') as imgfile:
                img = Image.open(imgfile)
                img.load()
            if img.size[0] != IMAGE_FRAME_WIDTH or img.size[1] != FRAME_HEIGHT:
                print 'Unexpected image size for %s = %s' % (
                    frame_file, [img.size])
                return self._frame

            if self._effect is not None:
                if not self._effect(img, self.elapsed_time):
                    self._effect = None

            if _SCREEN_FRAME_WIDTH / IMAGE_FRAME_WIDTH == 2:
                # Pre-split images, just copy them twice.
                self._frame = Image.new(
                    'RGB', (_SCREEN_FRAME_WIDTH, FRAME_HEIGHT))
                self._frame.paste(img, (0, 0))
                self._frame.paste(img, (IMAGE_FRAME_WIDTH, 0))
            elif self._split_sides:
                sub_img = img.resize((img.size[0] / 2, img.size[1]))
                #sub_img = img.crop((
                #    img.size[0] / 4, 0, img.size[0] * 3 / 4, img.size[1]))
                self._frame = Image.new('RGB', img.size)
                self._frame.paste(sub_img, (0, 0))
                self._frame.paste(sub_img, (sub_img.size[0], 0))
            else:
                self._frame = img

            self._prev_frame_file = frame_file
            self._tcl.send_frame(list(self._frame.getdata()))
            duration_ms = int(round((time.time() - start_time) * 1000))

            if frame_num >= self._prev_frame_num:
                self._skipped_frame_count += (
                    frame_num - self._prev_frame_num - 1)
            else:
                self._reset_stats()
            self._frame_render_count += 1
            self._prev_frame_num = frame_num
            frame_delay = elapsed_time - float(frame_num) / FPS
            self._frame_delay_stats.add(frame_delay * 1000)
            self._render_durations.add(duration_ms)

            return self._frame

    def _reset_stats(self):
        self._prev_frame_num = -1
        self._frame_render_count = 0
        self._skipped_frame_count = 0
        self._frame_delay_stats = Stats()
        self._render_durations = Stats()

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


