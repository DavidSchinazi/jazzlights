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


def _preprocess_video(src_path, name, audio_files):
  print ''
  print 'Processing:', name

  outpath = os.path.join(CLIPS_DIR, name)

  audio_codec = _get_audio_codec(src_path)
  need_m4a = audio_codec == 'aac'
  if need_m4a:
    audio_files.append(name + '.m4a')
  else:
    audio_files.append(name + '.' + audio_codec)

  if not _needs_processing(src_path, name):
    return

  print ''

  _remove_output(name)

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
    os.unlink(audio_stream_file)

  _create_stamp_file(src_path, name);


def _preprocess_audio(src_path, name, audio_files):
  print ''
  print 'Processing:', name

  basename = os.path.basename(src_path)
  audio_files.append(basename)

  if not _needs_processing(src_path, name):
    return

  _remove_output(name)
  shutil.copyfile(src_path, os.path.join(CLIPS_DIR, basename))
  _create_stamp_file(src_path, name);


def _needs_processing(src_path, name):
  src_time = os.path.getmtime(src_path)
  stamp_file = os.path.join(CLIPS_DIR, name + '.stamp')
  if os.path.exists(stamp_file):
    src_time_str = time.ctime(src_time)
    stamp_time_str = time.ctime(os.path.getmtime(stamp_file))
    if src_time_str == stamp_time_str:
      print 'Already have processed output %s' % (stamp_time_str)
      return False
    print 'Stale stamp file, will re-process (stamp=%s file=%s)' % (
        stamp_time_str, src_time_str)
  return True


def _create_stamp_file(src_path, name):
  src_time = os.path.getmtime(src_path)
  stamp_file = os.path.join(CLIPS_DIR, name + '.stamp')
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


def _remove_one_output(name, ext):
  path = os.path.join(CLIPS_DIR, name + ext)
  if not os.path.exists(path):
    return False
  if os.path.isdir(path):
    shutil.rmtree(path)
  else:
    os.unlink(path)
  return True


def _remove_output(name):
  has_removed = _remove_one_output(name, '.stamp')
  has_removed |= _remove_one_output(name, '.mp3')
  has_removed |= _remove_one_output(name, '.aac')
  has_removed |= _remove_one_output(name, '.m4a')
  has_removed |= _remove_one_output(name, '')
  if has_removed:
    print 'Removed parts of "%s"' % name


def _is_audio_file(path):
  return path.endswith('.m4a')


def main():
  if len(sys.argv) < 2:
    print 'Usage: %s <source_video_dir>' % sys.argv[0]
    exit(0)

  indir = sys.argv[1]

  for d in (CLIPS_DIR, PLAYLISTS_DIR):
    if not os.path.exists(d):
      os.makedirs(d)

  src_paths = glob.glob(indir + '/*.mp4')
  src_paths += glob.glob(indir + '/*.m4a')

  names = []
  audio_files = []
  for src_path in src_paths:
    name = os.path.splitext(os.path.basename(src_path))[0]
    names.append(name)
    if _is_audio_file(src_path):
      _preprocess_audio(src_path, name, audio_files)
    else:
      _preprocess_video(src_path, name, audio_files)

  with open(os.path.join(PLAYLISTS_DIR, 'playlist.m3u'), 'w') as playlist:
    for audio_file in audio_files:
      playlist.write(audio_file + '\n')

  print 'Removing outdated output'
  for d in os.listdir(CLIPS_DIR):
    basename = os.path.splitext(d)[0]
    if basename not in names:
      _remove_output(basename)

  # TODO(igorc): Consider auto-backing up new results in a tar/gz file.

  sys.exit(0)

