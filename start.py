#!/usr/bin/python
#
# Start dfplayer.

import argparse
import subprocess

def main():
  arg_parser = argparse.ArgumentParser(description='Start player')
  arg_parser.add_argument('--gdb', action='store_true')
  args = arg_parser.parse_args()

  if args.gdb:
    subprocess.check_call(['gdb', '-ex', 'run', '--args', 'env/bin/python', 'env/bin/dfplayer', '--listen=0.0.0.0:8080'])
  else:
    subprocess.check_call(['env/bin/dfplayer', '--listen=0.0.0.0:8080'])

main()

