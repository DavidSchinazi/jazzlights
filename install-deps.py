#!/usr/bin/python
#
# Installs useful dependencies for dfplayer development.

import os
import subprocess
import sys

_PILLOW_DEPS = [
    'libfreetype6-dev',
    'libjpeg8-dev',
    'liblcms2-dev',
    'libtiff4-dev',
    'libwebp-dev',
    'tcl-dev',
    'tk-dev',
    'zlib1g-dev',
    ]

_PLAYER_DEPS = [
    'libav-tools',
    'mpd',
    'python-dev',
    'python-mpd',
    'python-setuptools',
    'python-tk',
    ]

_USEFUL_PACKAGES = [
    'gimp',
    'git',
    'synaptic',
    'vim',
    ]

if os.geteuid() != 0:
  print 'ERROR: Please run as root'
  sys.exit(1)

subprocess.check_call(['apt-get', 'install', '-y'] + _PILLOW_DEPS)
subprocess.check_call(['apt-get', 'install', '-y'] + _PLAYER_DEPS)
subprocess.check_call(['apt-get', 'install', '-y'] + _USEFUL_PACKAGES)

subprocess.check_call(['easy_install', 'pip'])

# PIL and Pillow cannot coexist.
# subprocess.check_call(['pip', 'uninstall', 'PIL'])

subprocess.check_call(['pip', 'install', 'pillow'])
subprocess.check_call(['pip', 'install', 'virtualenv'])

print '=== SUCCESS ==='

