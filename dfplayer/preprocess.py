# -*- coding: utf-8 -*-
import os
import sys
import glob
import logging
import subprocess

from util import catch_and_log
from player import FPS, FRAME_WIDTH, FRAME_HEIGHT
from player import CLIPS_DIR, PLAYLISTS_DIR


def main():
    if len(sys.argv) < 2:
        print 'Usage: %s <source_video_dir>' % sys.argv[0]
        exit(0)
    else:
        indir = sys.argv[1]

    for d in (CLIPS_DIR, PLAYLISTS_DIR):
        if not os.path.exists(d):
            os.makedirs(d)

    with open(PLAYLISTS_DIR + 'playlist.m3u', 'w') as playlist:
        for infile in glob.glob(indir + '/*.mp4'):
            basename = os.path.basename(infile)[:-4]
            playlist.write(basename + '.mp3\n')
            outpath = CLIPS_DIR + basename
            if not os.path.exists(outpath):
                os.makedirs(outpath)

            start_t = 0
            duration = 60

            subprocess.check_call(['avconv', '-y',
                '-ss', str(start_t),
                '-i', infile,
                '-t', str(duration),
                '-r', str(FPS),
                '-vf',
                'scale=%(FRAME_WIDTH)d:-1,crop=%(FRAME_WIDTH)d:%(FRAME_HEIGHT)d'
                     % globals(),
                outpath + '/frame%5d.jpg',
                ])
            subprocess.check_call(['avconv', '-y',
                '-ss', str(start_t),
                '-i', infile,
                '-t', str(duration),
                outpath + '.mp3',
                ])
    exit(0)
