#!/usr/bin/python
#

import os
import subprocess
import sys

if len(sys.argv) < 2:
  print 'Usage: %s <dir>' % sys.argv[0]
  exit(0)

directory = sys.argv[1]

if os.path.exists(directory + '/CMakeCache.txt'):
  os.unlink(directory + '/CMakeCache.txt')

build_dir = directory + '/build'

if not os.path.exists(build_dir):
  os.mkdir(build_dir)

subprocess.check_call(['cmake', '-L', '..'], cwd=build_dir)
subprocess.check_call(['make', '-j', '2'], cwd=build_dir)

