#!/usr/bin/python
#
# Rebuilds virtualenv after pip changes.

import os
import shutil
import subprocess
import sys

def move_dir(src_dir, dst_dir):
  if not os.path.exists(src_dir):
    return True
  if os.path.exists(dst_dir):
    print 'ERROR: Cannot overwrite:', dst_dir
    return False
  os.rename(src_dir, dst_dir)
  return True


def backup(dir, reverse):
  src_dir = 'env/' + dir
  backup_dir = '_backup_' + dir
  if reverse:
    return move_dir(backup_dir, src_dir)
  else:
    return move_dir(src_dir, backup_dir)


if os.geteuid() == 0:
  print 'ERROR: Please run as regular user'
  sys.exit(1)

if not backup('clips', False):
  sys.exit(1)
if not backup('playlists', False):
  backup('clips', True)
  sys.exit(1)

shutil.rmtree('env')

subprocess.check_call(['make', 'develop'])

backup('clips', True)
backup('playlists', True)

print '=== SUCCESS ==='

