.PHONY: all clean install

CXX = c++ 
CXX_FLAGS = -std=c++11 -Isrc
BUILDDIR = build

ARDUINO_HEADERS = $(shell find src/arduinolib -name *.h -type f)
ARDUINO_SOURCES = $(shell find src/arduinolib -name *.cpp -type f)
SERVER_HEADERS = $(shell find src/server -name *.h -type f)
SERVER_SOURCES = $(shell find src/server -name *.cpp -type f)
DEMO_SOURCES = src/demo/demo.cpp

all: $(BUILDDIR)/demo

$(BUILDDIR)/demo: $(ARDUINO_HEADERS) $(ARDUINO_SOURCES) $(SERVER_HEADERS) $(SERVER_SOURCES) $(DEMO_SOURCES)
	mkdir -p $(dir $@)
	$(CXX) $(CXX_FLAGS) `pkg-config --cflags glfw3` $(DEMO_SOURCES) $(SERVER_SOURCES) -o $@  `pkg-config --static --libs glfw3`

clean:
	rm -rf build

