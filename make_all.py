#!/usr/bin/python
#
# Builds everything.

import os
import subprocess

def main():
  subprocess.check_call(['git', 'stash'], cwd='projectm')
  subprocess.check_call(['git', 'remote', 'update'])
  subprocess.check_call(['git', 'rebase', 'origin'])
  subprocess.check_call(['git', 'submodule', 'update', '--init'])

  if os.path.exists('dfplayer/libprojectM.so.2'):
    os.unlink('dfplayer/libprojectM.so.2')

  subprocess.check_call(['make', 'cpp'])
  
main()

