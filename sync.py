#!/usr/bin/python
#

import subprocess
import sys

subprocess.check_call(['git', 'submodule', 'update', '--init'])

