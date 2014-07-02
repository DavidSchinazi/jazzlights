# -*- coding: utf-8 -*-
#
# Loads and builds frame images.

import collections
import os

from PIL import Image

from .effect import load as load_effect
from .util import get_time_millis


class Frame(object):

  def __init__(self, timestamp, frame_num, current_ms, image):
    self.timestamp = timestamp
    self.frame_num = frame_num
    self.current_ms = current_ms
    self.image = image
    self.rendered = False


class FrameSource(object):

  def __init__(self, fps, screen_width, frame_width, frame_height,
               clips_dir):
    self._fps = fps
    self._screen_width = screen_width
    self._frame_width = frame_width
    self._frame_height = frame_height
    self._clips_dir = clips_dir
    self._effect = None
    self._split_sides = True

    self._prefetch_size = fps / 2
    self._clear_cache('')

  def play_effect(self, name, **kwargs):
    self._effect = load_effect(name, **kwargs)

  def stop_effect(self):
    self._effect = None

  def toggle_split_sides(self):
    self._split_sides = not self._split_sides

  def prefetch(self, clip_name, elapsed_sec):
    baseline_ms = get_time_millis()
    if clip_name != self._clip_name:
      self._clear_cache(clip_name)
    prev_frame_num = None
    # Remove outdated frames.
    while len(self._frames) > 1:
      if (self._frames[0].timestamp >= elapsed_sec or
          self._frames[1].timestamp >= elapsed_sec):
        break  # Keep one frame that is stale, so we can show something.
      self._frames.popleft()
    # Find the newest frame.
    if self._frames:
      first_frame = self._frames[0]
      prev_frame = self._frames[-1]
      if (elapsed_sec - prev_frame.timestamp) > 0.5:
        print 'Frame cache is behind, flushing; clip="%s", delay=%s' % (
            self._clip_name, elapsed_sec - prev_frame.timestamp)
        self._clear_cache(clip_name)
      elif (first_frame.timestamp - elapsed_sec) > 0.5:
        print 'Frame cache is ahead, flushing; clip="%s", delay=%s' % (
            self._clip_name, first_frame.timestamp - elapsed_sec)
        self._clear_cache(clip_name)
      else:
        prev_frame_num = prev_frame.frame_num
    if prev_frame_num is None:
      # The numbers in file system start with 1.
      prev_frame_num = int(elapsed_sec * self._fps)
    # Load up to 2 frames at a time to avoid seeking delays.
    max_to_load = min(self._frames.maxlen - len(self._frames), 2)
    max_to_skip = self._fps / 2
    while max_to_load and max_to_skip:
      frame_num = prev_frame_num + 1
      timestamp = float(frame_num) / self._fps
      current_ms = baseline_ms + int((timestamp - elapsed_sec) * 1000.0)
      frame = self._load_frame(frame_num, timestamp, current_ms)
      prev_frame_num = frame_num
      if not frame:
        max_to_skip -= 1
        continue
      max_to_load -= 1
      self._frames.append(frame)
      newest_frame = frame
    if not max_to_skip:
      print 'Skipped a lot of frames in %s / %s!' % (
          self._clip_name, prev_frame_num)

  def get_cached_frames(self):
    return list(self._frames)

  def get_cache_size(self):
    return len(self._frames)

  def _clear_cache(self, clip_name):
    self._clip_name = clip_name
    self._frames = collections.deque(maxlen=self._prefetch_size)

  def get_frame_at(self, elapsed_sec):
    prev_frame = None
    for f in self._frames:
      if f.timestamp > elapsed_sec:
        return prev_frame if prev_frame else f
    return self._frames[-1] if self._frames else None

  def load_frame_file(self, clip_name, frame_num):
    self._clear_cache(clip_name)
    timestamp = float(frame_num) / self._fps
    frame = self._load_frame(frame_num, timestamp, get_time_millis())
    if frame:
      return frame.image
    return None

  def _load_frame(self, frame_num, timestamp, current_ms):
    path = self._clips_dir + '%s/frame%06d.jpg' % (
        self._clip_name, frame_num + 1)
    if not os.path.exists(path):
      print 'No file', path
      return None

    with open(path, 'rb') as imgfile:
      src_img = Image.open(imgfile)
      src_img.load()
    if (src_img.size[0] != self._frame_width or
        src_img.size[1] != self._frame_height):
      print 'Unexpected image size for %s = %s' % (path, [src_img.size])
      return None

    if self._effect is not None:
      # TODO(igorc): Do not transpose text effect.
      if not self._effect(src_img, current_ms):
        self._effect = None

    if self._screen_width / self._frame_width == 2:
      # Pre-split images, just copy them twice.
      frame_img = Image.new(
          'RGB', (self._screen_width, self._frame_height))
      frame_img.paste(src_img, (self._frame_width, 0))
      flip_img = src_img.transpose(Image.FLIP_LEFT_RIGHT)
      frame_img.paste(flip_img, (0, 0))
    elif self._split_sides:
      sub_img = src_img.resize((src_img.size[0] / 2, src_img.size[1]))
      #sub_img = src_img.crop((
      #    src_img.size[0] / 4, 0, src_img.size[0] * 3 / 4, src_img.size[1]))
      frame_img = Image.new('RGB', src_img.size)
      frame_img.paste(sub_img, (0, 0))
      frame_img.paste(sub_img, (sub_img.size[0], 0))
    else:
      frame_img = src_img

    return Frame(timestamp, frame_num, current_ms, frame_img)

