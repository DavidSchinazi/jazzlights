#!/usr/bin/python
#
# Installs useful dependencies for dfplayer development.

import os
import subprocess
import sys

_REQUIRED_PACKAGES = [
    'gimp',
    'git',
    'libav-tools',
    'libfreetype6-dev',
    'libjpeg8-dev',
    'liblcms2-dev',
    'libtiff4-dev',
    'libwebp-dev',
    'mpd',
    'python-dev',
    'python-mpd',
    'python-setuptools',
    'python-tk',
    'synaptic',
    'tcl-dev',
    'tk-dev',
    'vim',
    'zlib1g-dev',
    ]

if os.geteuid() != 0:
  print 'ERROR: Please run as root'
  sys.exit(1)

subprocess.check_call([
    'apt-get', 'install', '-y'] + _REQUIRED_PACKAGES)

subprocess.check_call(['easy_install', 'pip'])

# PIL and Pillow cannot coexist.
# subprocess.check_call(['pip', 'uninstall', 'PIL'])

subprocess.check_call(['pip', 'install', 'pillow'])
subprocess.check_call(['pip', 'install', 'virtualenv'])

print '=== SUCCESS ==='

