cmake_minimum_required(VERSION 2.8)
set(CMAKE_MACOSX_RPATH 1)
project(DFSparks)

set(LIB_SOURCES 
	src/DFSparks.h 
	src/dfsparks/color.cpp
	src/dfsparks/color.h
	src/dfsparks/effect.cpp
	src/dfsparks/effect.h
	src/dfsparks/effects/blink.h
	src/dfsparks/effects/flame.cpp
	src/dfsparks/effects/flame.h
	src/dfsparks/effects/glitter.h
	src/dfsparks/effects/rainbow.h
	src/dfsparks/effects/slantbars.h
	src/dfsparks/log.cpp
	src/dfsparks/log.h
	src/dfsparks/math.cpp
	src/dfsparks/math.h
	src/dfsparks/network.cpp
	src/dfsparks/network.h
	src/dfsparks/networks/esp8266wifi.cpp
	src/dfsparks/networks/esp8266wifi.h
	src/dfsparks/networks/udpsocket.cpp
	src/dfsparks/networks/udpsocket.h
	src/dfsparks/palette.cpp
	src/dfsparks/palette.h
	src/dfsparks/player.cpp
	src/dfsparks/player.h
	src/dfsparks/playlist.cpp
	src/dfsparks/playlist.h
	src/dfsparks/timer.cpp
	src/dfsparks/timer.h
)

set(DEMO_SOURCES extras/demo.cpp)

set(CMAKE_CXX_FLAGS "-g -fPIC -std=c++11 -Wall -Wextra -DVERSION=\"0.1\"")

# ARDUINO LIBRARY
add_custom_target(arduinolib ALL DEPENDS ${CMAKE_BINARY_DIR}/DFSparks.zip)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/DFSparks.zip 
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMAND echo Building ${CMAKE_BINARY_DIR}/DFSparks.zip
	COMMAND zip -r ${CMAKE_BINARY_DIR}/DFSparks.zip src examples extras --include "*.cpp" "*.h" "*.py" "*.ino" 
	DEPENDS ${LIB_SOURCES}
)

# PC LIBRARY
add_library(dfsparks SHARED ${LIB_SOURCES})
target_include_directories(dfsparks PUBLIC src)

# DEMO
add_executable(dfsparks_demo EXCLUDE_FROM_ALL ${DEMO_SOURCES})
find_package(glfw3 3.2)
target_link_libraries(dfsparks_demo dfsparks glfw)
target_include_directories(dfsparks_demo PRIVATE src)

# PLATFORM DETAILS
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	include_directories(/usr/local/include)
	target_link_libraries(dfsparks_demo "-L/usr/local/lib -framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	target_link_libraries(dfsparks_demo "-L/usr/local/lib -lGL")	
endif()

install(TARGETS dfsparks
	LIBRARY DESTINATION lib
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/dfsparks/
		DESTINATION include/dfsparks
        FILES_MATCHING PATTERN "*.h"
  )

# Test deployment
add_custom_target(test-deploy 
	COMMAND docker build -t dfsparks .
	#COMMAND docker run -i -t dfsparks
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
