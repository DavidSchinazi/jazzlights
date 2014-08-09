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

subprocess.check_call(['cmake', '.'], cwd=directory)
subprocess.check_call(['make', '-j', '2'], cwd=directory)

