.PHONY: all clean clean-pyc develop run

all: develop clean-pyc

env:
	virtualenv env

develop: env
	env/bin/python setup.py develop

run: develop
	env/bin/dfplayer --listen 127.0.0.1:8080

clean-pyc:
	find . -name '*.pyc' -exec rm -f {} +
	find . -name '*.pyo' -exec rm -f {} +
	find . -name '*~' -exec rm -f {} +

clean: clean-pyc
	rm -rf dfplayer.egg-info
	rm -rf build
	rm -rf env
