#!/usr/bin/python
#
# Start dfplayer.

import subprocess

subprocess.check_call(['env/bin/dfplayer', '--listen=0.0.0.0:8080'])

