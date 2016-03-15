.PHONY: all clean install

CXX = c++ 
CXX_FLAGS = -std=c++11 -I src -I src/arduinolib/DFSparks
BUILDDIR = build

ARDUINO_HEADERS = $(shell find src/arduinolib -name *.h -type f)
ARDUINO_SOURCES = $(shell find src/arduinolib -name *.cpp -type f)
ARDUINO_LIB_PATH = ~/Documents/Arduino/libraries
SERVER_HEADERS = $(shell find src/server -name *.h -type f)
SERVER_SOURCES = $(shell find src/server -name *.cpp -type f)
DEMO_SOURCES = src/demo/demo.cpp

all: $(BUILDDIR)/demo $(BUILDDIR)/DFSparks.zip

$(BUILDDIR)/demo: $(ARDUINO_HEADERS) $(ARDUINO_SOURCES) $(SERVER_HEADERS) $(SERVER_SOURCES) $(DEMO_SOURCES)
	mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) `pkg-config --cflags glfw3` $(DEMO_SOURCES) $(SERVER_SOURCES) -o $@  `pkg-config --static --libs glfw3`

$(BUILDDIR)/DFSparks.zip: $(shell find src/arduinolib/DFSparks -type f) 
	cd src/arduinolib && zip ../../$@ DFSparks/*

install: $(BUILDDIR)/DFSparks.zip
	mkdir -p $(ARDUINO_LIB_PATH)
	cp $(BUILDDIR)/DFSparks.zip $(ARDUINO_LIB_PATH) && cd $(ARDUINO_LIB_PATH) && rm -rf DFSParks && unzip DFSParks.zip && rm DFSParks.zip

clean:
	rm -rf build

