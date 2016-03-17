.PHONY: all clean install

VERSION = 0.1

BUILDDIR = build
CXX = c++ 
CXX_FLAGS = -std=c++11 -Isrc -Isrc/arduinolib/DFSparks -DVERSION=\"$(VERSION)\" -L$(BUILDDIR)

ARDUINO_LIB_INSTALL_PATH = ~/Documents/Arduino/libraries
ARDUINOLIB_HEADERS = $(shell find src/arduinolib -name *.h -type f)
ARDUINOLIB_SOURCES = $(shell find src/arduinolib -name *.cpp -type f)
SOCKETLIB_HEADERS = $(shell find src/socketlib -name *.h -type f)
SOCKETLIB_SOURCES = $(shell find src/socketlib -name *.cpp -type f)
DEMO_SOURCES = $(shell find src/demo -name *.cpp -type f)

all: $(BUILDDIR)/dfsparks_demo $(BUILDDIR)/DFSparks.zip

$(BUILDDIR)/DFSparks.zip: $(shell find src/arduinolib/DFSparks -type f) 
	mkdir -p $(dir $@)
	cd src/arduinolib && zip -r ../../$@ DFSparks

# $(BUILDDIR)/libdfsparks.a: $(SOCKETLIB_HEADERS) $(SOCKETLIB_SOURCES) $(ARDUINOLIB_HEADERS) $(ARDUINOLIB_SOURCES) 
# 	mkdir -p $(dir $@)
# 	$(CXX) $(CXX_FLAGS) $(filter %.cpp, $^) -c -o $@

$(BUILDDIR)/dfsparks_demo: $(SOCKETLIB_HEADERS) $(SOCKETLIB_SOURCES) $(ARDUINOLIB_HEADERS) $(ARDUINOLIB_SOURCES) $(DEMO_SOURCES)
	mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) `pkg-config --cflags glfw3` $(filter %.cpp %.a, $^) -o $@ `pkg-config --static --libs glfw3`

install: $(BUILDDIR)/DFSparks.zip
	mkdir -p $(ARDUINO_LIB_INSTALL_PATH)
	cp $(BUILDDIR)/DFSparks.zip $(ARDUINO_LIB_PATH) && cd $(ARDUINO_LIB_PATH) && rm -rf DFSParks && unzip DFSParks.zip && rm DFSParks.zip

clean:
	rm -rf build

