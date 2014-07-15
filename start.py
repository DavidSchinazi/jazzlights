#!/usr/bin/python
#
# Start dfplayer.

import argparse
import os
import shutil
import subprocess

def main():
  arg_parser = argparse.ArgumentParser(description='Start player')
  arg_parser.add_argument('--gdb', action='store_true')
  args = arg_parser.parse_args()

  shutil.copyfile(
      'dfplayer/asoundrc', '/home/' + os.getlogin() + '/.asoundrc')

  params = ['env/bin/dfplayer', '--listen=0.0.0.0:8080']
  if args.gdb:
    subprocess.check_call(
        ['gdb', '-ex', 'run', '--args', 'env/bin/python'] + params)
        #['gdb', '--args', 'env/bin/python'] + params)
  else:
    subprocess.check_call(params)

main()

