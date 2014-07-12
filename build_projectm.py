#!/usr/bin/python
#

import subprocess

subprocess.check_call(['cmake', '.'], cwd='projectm/src/libprojectM')
subprocess.check_call(['make', '-j', '2'], cwd='projectm/src/libprojectM')

