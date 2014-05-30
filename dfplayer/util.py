# -*- coding: utf-8 -*-
import os
import pdb
import logging
import functools
import traceback
import contextlib

PACKAGE_DIR = os.path.abspath(os.path.dirname(__file__))
PROJECT_DIR = os.path.dirname(PACKAGE_DIR)
VENV_DIR = os.path.join(PROJECT_DIR, 'env')


class ExceptionLogger(object):

    def __init__(self, level=logging.ERROR, catch=Exception,
        ignore=(KeyboardInterrupt, SystemExit)):
        self.level = level
        self.catch = catch
        self.ignore = ignore

    def __call__(self, func):

        @functools.wraps(func)
        def decorated(*args, **kwargs):
            with self:
                return func(*args, **kwargs)
        return decorated

    def __enter__(self):
        pass

    def __exit__(self, errtype, errvalue, errtrace):
        if errvalue is not None:
            if not isinstance(errvalue, self.catch) \
                or isinstance(errvalue, self.ignore):
                return False
            else:
                logging.log(self.level, errtype.__name__ + ': '
                            + ''.join(traceback.format_exception(errtype,
                            errvalue, errtrace)))
                return True


def catch_and_log(*args, **kwargs):
    return ExceptionLogger(*args, **kwargs)


