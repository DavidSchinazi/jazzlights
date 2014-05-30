.PHONY: all clean very-clean develop run

all: develop clips 

develop: env
	env/bin/python setup.py develop

run: develop
	env/bin/dfplayer --listen 127.0.0.1:8080

clips: develop
	-rm -rf env/clips
	-rm -rf env/playlists
	env/bin/dfprepr clips

clean:
	find . -name '*.pyc' -exec rm -f {} +
	find . -name '*.pyo' -exec rm -f {} +
	find . -name '*~' -exec rm -f {} +
	rm -rf dfplayer.egg-info
	rm -rf build
	rm -rf env/mpd
	rm -rf env/clips

very-clean: clean
	rm -rf env

env:
	virtualenv env

