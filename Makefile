.PHONY: all clean very-clean develop run

all: develop cpp clips

develop: env
	env/bin/python setup.py develop

cpp:
	swig -python -c++ `python-config --includes` dfplayer/renderer.i
	g++ -std=c++11 -Wall -Wextra -g -ggdb3 -fPIC -shared `python-config --includes` -Wl,-rpath,./dfplayer -o dfplayer/_renderer_cc.so dfplayer/tcl_renderer.cc dfplayer/visualizer.cc dfplayer/input_alsa.cc dfplayer/utils.cc dfplayer/renderer_wrap.cxx -lpthread -lm -ldl -lasound -lGL dfplayer/libprojectM.so.2

run: develop cpp
	env/bin/dfplayer --listen 127.0.0.1:8080

clips: develop
	-rm -rf env/playlists
	env/bin/dfprepr clips

dfplayer/libprojectM.so.2:
	./build_cmake.py projectm/src/libprojectM
	cp projectm/src/libprojectM/libprojectM.so dfplayer/libprojectM.so.2

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

