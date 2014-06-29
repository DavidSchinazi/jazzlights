.PHONY: all clean very-clean develop run

all: develop cpp clips

develop: env
	env/bin/python setup.py develop

cpp: develop
	swig -python -c++ `python-config --includes` dfplayer/tcl_renderer.i
	g++ -std=c++11 -Wall -g -ggdb -fPIC -rdynamic -shared `python-config --includes` -pthread -o dfplayer/tcl_renderer_cc.so dfplayer/tcl_renderer.cc dfplayer/tcl_renderer_wrap.cxx

run: develop cpp
	env/bin/dfplayer --listen 127.0.0.1:8080

clips: develop
	-rm -rf env/playlists
	env/bin/dfprepr clips

clean:
	find . -name '*.pyc' -exec rm -f {} +
	find . -name '*.pyo' -exec rm -f {} +
	find . -name '*~' -exec rm -f {} +
	rm -rf dfplayer.egg-info
	rm -rf build
	rm -rf env/mpd

very-clean: clean
	rm -rf env

env:
	virtualenv env

