#!/usr/bin/python
#
# Start dfplayer.

import argparse
import os
import shutil
import subprocess
import sys
import time

_PROJ_DIR = os.path.dirname(__file__)


def main():
  os.chdir(_PROJ_DIR)

  os.environ['LD_LIBRARY_PATH'] = '/lib:/usr/lib:/usr/local/lib'
  
  arg_parser = argparse.ArgumentParser(description='Start player')
  arg_parser.add_argument('--gdb', action='store_true')
  arg_parser.add_argument('--no-reset', action='store_true')
  arg_parser.add_argument('--disable-net', action='store_true')
  arg_parser.add_argument('--mpd', action='store_true')
  arg_parser.add_argument('--disable-fin', action='store_true')
  arg_parser.add_argument('--max', action='store_true')
  arg_parser.add_argument('--no-sound', action='store_true')
  arg_parser.add_argument('--no-sound-config', action='store_true')
  arg_parser.add_argument('--prod', action='store_true')
  arg_parser.add_argument('--enable-kinect', action='store_true')
  args = arg_parser.parse_args()

  if args.prod:
    print 'dfplayer is sleeping for 30 seconds before startup'
    time.sleep(30)

  if not args.no_sound_config and not args.no_sound:
    shutil.copyfile(
        'dfplayer/asoundrc.sample', '/home/' + os.getlogin() + '/.asoundrc')

  params = ['env/bin/dfplayer', '--listen=0.0.0.0:8081']
  if args.no_reset:
    params.append('--no-reset')
  if args.no_sound:
    params.append('--no-sound')
  if args.disable_net:
    params.append('--disable-net')
  if args.disable_fin:
    params.append('--disable-fin')
  if args.enable_kinect or args.prod:
    params.append('--enable-kinect')
  if args.mpd:
    params.append('--mpd')
  if args.max or args.prod:
    params.append('--max')

  try:
    if args.gdb:
      subprocess.check_call(
          ['gdb', '-ex', 'run', '--args', 'env/bin/python'] + params)
          #['gdb', '--args', 'env/bin/python'] + params)
    else:
      subprocess.check_call(params)
  except KeyboardInterrupt:
    print 'Player is exiting via KeyboardInterrupt'
  except Exception, err:
    print sys.exc_info()[0]

  if args.prod:
    print 'dfplayer has exited and start.py script is now sleeping'
    time.sleep(3600)

main()

