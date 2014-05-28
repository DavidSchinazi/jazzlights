.PHONY: all clean clean-pyc develop run start-mpd stop-mpd clips

all: develop clean-pyc

env:
	virtualenv env

develop: env
	env/bin/python setup.py develop

run: develop
	env/bin/dfplayer --listen 127.0.0.1:8080

start-mpd: 
	-mkdir env/mpd/
	-rm env/mpd/tag_cache
	touch env/mpd/tag_cache
	mpd dfplayer/conf/mpd.cfg

stop-mpd: 
	-mpd --kill dfplayer/conf/mpd.cfg

restart-mpd: stop-mpd start-mpd

clean-pyc:
	find . -name '*.pyc' -exec rm -f {} +
	find . -name '*.pyo' -exec rm -f {} +
	find . -name '*~' -exec rm -f {} +

clean: clean-pyc
	rm -rf dfplayer.egg-info
	rm -rf build
	rm -rf env

clips: develop
	env/bin/dfprepr	 clips env/clips
