.PHONY: all clean very-clean develop run


SOURCES := \
	dfplayer/tcl_renderer.cc \
	dfplayer/visualizer.cc \
	dfplayer/kinect.cc \
	dfplayer/utils.cc \
	dfplayer/renderer_wrap.cxx \
	src/effects/fishify.cc \
	src/effects/passthrough.cc \
	src/effects/rainbow.cc \
	src/effects/wearable.cc \
	src/model/effect.cc \
	src/model/image_source.cc \
	src/model/projectm_source.cc \
	src/tcl/tcl_controller.cc \
	src/tcl/tcl_manager.cc \
	src/util/input_alsa.cc \
	src/util/led_layout.cc \
	src/util/pixels.cc \
	src/util/time.cc

LINK_LIBS := \
	-lpthread -lm -ldl -lasound -lGL \
	-lopencv_core -lopencv_imgproc -lopencv_contrib -ldfsparks

LINK_DEPS := \
	dfplayer/libprojectM.so.2 \
	dfplayer/libkkonnect.so.0.1

COPTS := \
	-std=c++0x -Wall -Wextra \
	-g -ggdb3 -fPIC \
	-shared \
	`python-config --includes` \
	-Isrc \
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

dfplayer/libprojectM.so.2:
	./build_cmake.py projectm/src/libprojectM
	cp projectm/src/libprojectM/build/libprojectM.so dfplayer/libprojectM.so.2

dfplayer/libkkonnect.so.0.1:
	./build_cmake.py external/kkonnect
	cp external/kkonnect/build/lib/libkkonnect.so dfplayer/libkkonnect.so.0.1

clean:
	find . -name '*.pyc' -exec rm -f {} +
	find . -name '*.pyo' -exec rm -f {} +
	find . -name '*~' -exec rm -f {} +
	rm -rf dfplayer.egg-info
	rm -rf build
	rm -rf external/kkonnect/build
	rm -rf env/mpd

very-clean: clean
	rm -rf env

env:
	virtualenv env

