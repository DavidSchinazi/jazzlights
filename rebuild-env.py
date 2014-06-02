#!/usr/bin/python
#
# Rebuilds virtualenv after pip changes.

# TODO(igorc): Make this script pass rather than first fail
# and then having to do "make develop" and move the clips.
# It also seems to do poor job saving/restoring clips,
# as player complains about missing playlists.

import os
import shutil
import subprocess
import sys

_CLIPS_DST = 'env/clips'
_CLIPS_BAK = 'clips_bak'

if os.geteuid() == 0:
  print 'ERROR: Please run as regular user'
  sys.exit(1)

if os.path.exists(_CLIPS_DST):
  if os.path.exists(_CLIPS_BAK):
    print 'ERROR: Cannot overwrite:', _CLIPS_BAK
    sys.exit(1)
  os.rename(_CLIPS_DST, _CLIPS_BAK)

shutil.rmtree('env')

# subprocess.check_call(['pip', 'uninstall', 'pillow'])
# subprocess.check_call(['pip', 'install', 'pillow'])

subprocess.check_call(['make', 'develop'])

if os.path.exists(_CLIPS_BAK):
  os.rename(_CLIPS_BAK, _CLIPS_DST)

print '=== SUCCESS ==='

