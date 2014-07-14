.PHONY: all clean very-clean develop run

all: develop cpp clips

develop: env
	env/bin/python setup.py develop

cpp:
	swig -python -c++ `python-config --includes` dfplayer/tcl_renderer.i
	g++ -std=c++11 -Wall -g -ggdb -fPIC -rdynamic -shared -Xlinker -export-dynamic `python-config --includes` -lpthread -lm -ldl -o dfplayer/_tcl_renderer_cc.so dfplayer/tcl_renderer.cc dfplayer/tcl_renderer_wrap.cxx
	swig -python -c++ `python-config --includes` dfplayer/visualizer.i
#	g++ -std=c++11 -Wall -g -ggdb -fPIC -rdynamic -shared -Wl,-symbolic -export-dynamic `python-config --includes` -lpthread -lm -ldl -lprojectM -L dfplayer -Wl,-rpath,dfplayer -Wl,-rpath-link,dfplayer -o dfplayer/_visualizer_cc.so dfplayer/visualizer.cc dfplayer/input_alsa.cc dfplayer/visualizer_wrap.cxx
#	g++ -std=c++11 -Wall -g -ggdb -fPIC -shared `python-config --includes` -L dfplayer -L /usr/lib/x86_64-linux-gnu/mesa -Wl,-rpath,. -Wl,-rpath,/usr/lib/x86_64-linux-gnu/mesa -o dfplayer/_visualizer_cc.so dfplayer/visualizer.cc dfplayer/input_alsa.cc dfplayer/visualizer_wrap.cxx -lpthread -lm -ldl -lasound -lprojectM -lGL
	g++ -std=c++11 -Wall -Wextra -g -ggdb3 -fPIC -shared `python-config --includes` -L dfplayer -Wl,-rpath,. -o dfplayer/_visualizer_cc.so dfplayer/visualizer.cc dfplayer/input_alsa.cc dfplayer/visualizer_wrap.cxx -lpthread -lm -ldl -lasound -lprojectM -lGL

run: develop cpp
	env/bin/dfplayer --listen 127.0.0.1:8080

clips: develop
	-rm -rf env/playlists
	env/bin/dfprepr clips

dfplayer/libprojectM.so:
	./build_cmake.py projectm/src/libprojectM
	cp projectm/src/libprojectM/libprojectM.so dfplayer/libprojectM.so

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

