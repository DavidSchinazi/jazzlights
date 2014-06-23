# -*- coding: utf-8 -*-
#
# Processes clips into audio tracks and video images.

import glob
import logging
import os
import re
import shutil
import subprocess
import sys
import time

from player import FPS, IMAGE_FRAME_WIDTH, FRAME_HEIGHT
from player import CLIPS_DIR, PLAYLISTS_DIR
from util import catch_and_log


def _preprocess(src_path, basename, audio_files):
  print ''
  print 'Processing:', basename

  outpath = CLIPS_DIR + basename

  audio_codec = _get_audio_codec(src_path)
  need_m4a = audio_codec == 'aac'
  if need_m4a:
    audio_files.append(basename + '.m4a')
  else:
    audio_files.append(basename + '.' + audio_codec)

  src_time = os.path.getmtime(src_path)
  stamp_file = outpath + '.stamp'
  if os.path.exists(stamp_file):
    src_time_str = time.ctime(src_time)
    stamp_time_str = time.ctime(os.path.getmtime(stamp_file))
    if src_time_str == stamp_time_str:
      print 'Already have processed output %s' % (stamp_time_str)
      return
    print 'Stale stamp file, will re-process (stamp=%s file=%s)' % (
        stamp_time_str, src_time_str)

  print ''

  _remove_output(basename)

  if not os.path.exists(outpath):
    os.makedirs(outpath)

  start_t = 0
  duration = 60000

  subprocess.check_call(
      ['avconv', '-y',
       '-ss', str(start_t),
       '-i', src_path,
       '-t', str(duration),
       '-r', str(FPS),
       '-vf',
       'scale=%s:-1,crop=%s:%s' % (
           IMAGE_FRAME_WIDTH, IMAGE_FRAME_WIDTH, FRAME_HEIGHT),
       outpath + '/frame%6d.jpg',
      ])

  audio_stream_file = outpath + '.' + audio_codec
  subprocess.check_call(
      ['avconv', '-y',
       '-ss', str(start_t),
       '-i', src_path,
       '-t', str(duration),
       '-acodec', 'copy',
       audio_stream_file,
      ])

  if need_m4a:
    # Wrap in a container to allow seeking.
    subprocess.check_call(
        ['avconv', '-y',
         '-i', audio_stream_file,
         '-acodec', 'copy',
          outpath + '.m4a',
        ])
    # os.unlink(audio_stream_file)

  with open(stamp_file, 'w') as f:
    f.write('')
  os.utime(stamp_file, (src_time, src_time))


def _get_audio_codec(src_path):
  probe_str = subprocess.check_output(
      ['avprobe', src_path], stderr=subprocess.STDOUT)
  probe_str = probe_str.replace('\n', ' ')
  m = re.match(r'.*Audio:\s([0-9,a-z]+)\,.*', probe_str)
  if not m:
    raise Exception('Unable to get audio codec name for ' + src_path)
  return m.group(1)


def _remove_one_output(basename, ext):
  path = CLIPS_DIR + basename + ext
  if not os.path.exists(path):
    return False
  if os.path.isdir(path):
    shutil.rmtree(path)
  else:
    os.unlink(path)
  return True


def _remove_output(basename):
  has_removed = _remove_one_output(basename, '.stamp')
  has_removed |= _remove_one_output(basename, '.mp3')
  has_removed |= _remove_one_output(basename, '.aac')
  has_removed |= _remove_one_output(basename, '.m4a')
  has_removed |= _remove_one_output(basename, '')
  if has_removed:
    print 'Removed parts of "%s"' % basename


def main():
  if len(sys.argv) < 2:
    print 'Usage: %s <source_video_dir>' % sys.argv[0]
    exit(0)

  indir = sys.argv[1]

  for d in (CLIPS_DIR, PLAYLISTS_DIR):
    if not os.path.exists(d):
      os.makedirs(d)

  names = []
  audio_files = []
  for src_path in glob.glob(indir + '/*.mp4'):
    basename = os.path.splitext(os.path.basename(src_path))[0]
    names.append(basename)
    _preprocess(src_path, basename, audio_files)

  with open(PLAYLISTS_DIR + 'playlist.m3u', 'w') as playlist:
    for audio_file in audio_files:
      playlist.write(audio_file + '\n')

  print 'Removing outdated output'
  for d in os.listdir(CLIPS_DIR[:-1]):
    basename = os.path.splitext(d)[0]
    if basename not in names:
      _remove_output(basename)

  # TODO(igorc): Consider auto-backing up new results in a tar/gz file.

  sys.exit(0)

