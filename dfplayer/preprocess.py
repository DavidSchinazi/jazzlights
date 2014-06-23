# -*- coding: utf-8 -*-
#
# Processes clips into audio tracks and video images.

import glob
import logging
import os
import shutil
import subprocess
import sys
import time

from player import FPS, FRAME_WIDTH, FRAME_HEIGHT
from player import CLIPS_DIR, PLAYLISTS_DIR
from util import catch_and_log


def _preprocess(infile, basename):
  print ''
  print 'Processing:', basename

  outpath = CLIPS_DIR + basename

  infile_time = os.path.getmtime(infile)
  stamp_file = outpath + '.stamp'
  if os.path.exists(stamp_file):
    infile_time_str = time.ctime(infile_time)
    stamp_time_str = time.ctime(os.path.getmtime(stamp_file))
    if infile_time_str == stamp_time_str:
      print 'Already have processed output %s' % (stamp_time_str)
      return
    print 'Stale stamp file, will re-process (stamp=%s file=%s)' % (
        stamp_time_str, infile_time_str)

  print ''

  _remove_output(basename)

  if not os.path.exists(outpath):
    os.makedirs(outpath)

  start_t = 0
  duration = 60000

  subprocess.check_call(['avconv', '-y',
      '-ss', str(start_t),
      '-i', infile,
      '-t', str(duration),
      '-r', str(FPS),
      '-vf',
      'scale=%(FRAME_WIDTH)d:-1,crop=%(FRAME_WIDTH)d:%(FRAME_HEIGHT)d'
          % globals(),
      outpath + '/frame%6d.jpg',
      ])

  subprocess.check_call(['avconv', '-y',
      '-ss', str(start_t),
      '-i', infile,
      '-t', str(duration),
      outpath + '.mp3',
      ])

  with open(stamp_file, 'w') as f:
    f.write('')
  os.utime(stamp_file, (infile_time, infile_time))


def _remove_output(basename):
  has_removed = False
  outpath = CLIPS_DIR + basename
  if os.path.exists(outpath + '.stamp'):
    os.unlink(outpath + '.stamp')
    has_removed = True
  if os.path.exists(outpath + '.mp3'):
    os.unlink(outpath + '.mp3')
    has_removed = True
  if os.path.exists(outpath):
    shutil.rmtree(outpath)
    has_removed = True
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
  for infile in glob.glob(indir + '/*.mp4'):
    basename = os.path.splitext(os.path.basename(infile))[0]
    names.append(basename)
    _preprocess(infile, basename)

  with open(PLAYLISTS_DIR + 'playlist.m3u', 'w') as playlist:
    for basename in names:
      playlist.write(basename + '.mp3\n')

  print 'Removing outdated output'
  for d in os.listdir(CLIPS_DIR[:-1]):
    basename = os.path.splitext(d)[0]
    if basename not in names:
      _remove_output(basename)

  sys.exit(0)

