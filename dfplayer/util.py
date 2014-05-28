import os
import logging
import traceback

PACKAGE_DIR = os.path.abspath(os.path.dirname(__file__))
PROJECT_DIR = os.path.dirname(PACKAGE_DIR)


class ExceptionLogger(object):
    def __init__(self,level=logging.ERROR):
        self.level = level

    def __enter__(self):
        pass

    def __exit__(self, errtype, errvalue, traceback):
        if errvalue is not None:
            if isinstance(errvalue,KeyboardInterrupt) or\
                isinstance(errvalue,SystemExit):
                return False
            else:
                #logging.log(self.level,traceback.format_exception(errtype,errvalue).join(''))
                logging.log(self.level, "%s: %s", errtype.__name__, errvalue)
                return True


def catch_all(func,level=logging.ERROR):
    def decorator(*args,**kwargs):
        with ExceptionLogger():
            return func(*args,**kwargs)
    return decorator
