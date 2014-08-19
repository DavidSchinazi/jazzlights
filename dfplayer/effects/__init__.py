# -*- coding: utf-8 -*-
# Licensed under The MIT License
import os
import glob

def register_all():
    for f in glob.glob( os.path.dirname(__file__) + '/*.py' ):
        name = os.path.basename(f)
        if name.startswith('_'):
            continue 
        pkg = name[:-3]
        __import__( pkg, globals(), locals(), [], 1)

