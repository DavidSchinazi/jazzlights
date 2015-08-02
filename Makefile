.PHONY: all clean very-clean develop run

SOURCES := \
	dfplayer/tcl_renderer.cc \
	dfplayer/visualizer.cc \
	dfplayer/input_alsa.cc \
	dfplayer/kinect.cc \
	dfplayer/utils.cc \
	dfplayer/renderer_wrap.cxx

LINK_LIBS := \
	-lpthread -lm -ldl -lasound -lGL -lopencv_core -lopencv_imgproc -lopencv_contrib

LINK_DEPS := \
	dfplayer/libprojectM.so.2 \
	dfplayer/libfreenect.so.0.5 \
	dfplayer/libfreenect2.so

COPTS := \
	-std=c++0x -Wall -Wextra \
	-g -ggdb3 -fPIC \
	-shared `python-config --includes` \
	-Wl,-rpath,./dfplayer

all: develop cpp clips

develop: env
	env/bin/python setup.py develop

cpp: $(LINK_DEPS)
	swig -python -c++ `python-config --includes` dfplayer/renderer.i
	g++ $(COPTS) -o dfplayer/_renderer_cc.so $(SOURCES) $(LINK_LIBS) $(LINK_DEPS)

run: develop cpp
	env/bin/dfplayer --listen 127.0.0.1:8080

clips: develop
	-rm -rf env/playlists
	env/bin/dfprepr clips

projectm/src/libprojectM/build/libprojectM.so.2:
	./build_cmake.py projectm/src/libprojectM
	cp projectm/src/libprojectM/build/libprojectM.so projectm/src/libprojectM/build/libprojectM.so.2

libfreenect/build/lib/libfreenect.so.0.5:
	./build_cmake.py libfreenect
	cp libfreenect/build/lib/libfreenect.so libfreenect/build/lib/libfreenect.so.0.5

dfplayer/libprojectM.so.2:
	./build_cmake.py projectm/src/libprojectM
	cp projectm/src/libprojectM/build/libprojectM.so dfplayer/libprojectM.so.2

dfplayer/libfreenect.so.0.5:
	./build_cmake.py libfreenect
	cp libfreenect/build/lib/libfreenect.so dfplayer/libfreenect.so.0.5

dfplayer/libfreenect2.so:
	./build_cmake.py libfreenect2/examples/protonect
	cp libfreenect2/examples/protonect/lib/libfreenect2.so dfplayer/libfreenect2.so

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

