cmake_minimum_required(VERSION 2.8)
set(CMAKE_MACOSX_RPATH 1)
project(Unisparks)

set(LIB_SOURCES 
	src/Unisparks.h 
	src/unisparks/color.cpp
	src/unisparks/color.h
	src/unisparks/effect.cpp
	src/unisparks/effect.h
	src/unisparks/effects/flame.cpp
	src/unisparks/effects/flame.h
	src/unisparks/effects/simple.cpp
	src/unisparks/effects/simple.h
	src/unisparks/effects/system.cpp
	src/unisparks/effects/system.h
	src/unisparks/log.cpp
	src/unisparks/log.h
	src/unisparks/math.cpp
	src/unisparks/math.h
	src/unisparks/network.cpp
	src/unisparks/network.h
	src/unisparks/networks/arduinoethernet.cpp
	src/unisparks/networks/arduinoethernet.h
	src/unisparks/networks/esp8266wifi.cpp
	src/unisparks/networks/esp8266wifi.h
	src/unisparks/networks/udpsocket.cpp
	src/unisparks/networks/udpsocket.h
	src/unisparks/palette.cpp
	src/unisparks/palette.h
	src/unisparks/player.cpp
	src/unisparks/player.h
	src/unisparks/playlist.cpp
	src/unisparks/playlist.h
	src/unisparks/timer.cpp
	src/unisparks/timer.h
)

set(DEMO_SOURCES extras/demo.cpp)
set(CMAKE_CXX_FLAGS "-g -fPIC -std=c++11 -Wall -Wextra -DVERSION=\"0.1\"")

# PLATFORM DETAILS
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	include_directories(/usr/local/include)
	set(ARDUINO "/Applications/Arduino.app/Contents/MacOS/Arduino")
	set(ARDUINO_LIBS $ENV{HOME}"/Documents/Arduino/libraries/")
	set(DEMO_SYSTEM_LIBS "-L/usr/local/lib -framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo")
	set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(ARDUINO arduino)
	set(ARDUINO_LIBS $ENV{HOME}"/Arduino/libraries/")
	set(DEMO_SYSTEM_LIBS "-L/usr/local/lib -lGL")	
endif()

# ARDUINO LIBRARY
add_custom_target(arduinolib ALL DEPENDS ${CMAKE_BINARY_DIR}/Unisparks.zip)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/Unisparks.zip 
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	COMMAND echo Building ${CMAKE_BINARY_DIR}/Unisparks.zip
	COMMAND zip -r ${CMAKE_BINARY_DIR}/Unisparks.zip src examples extras --include "*.cpp" "*.h" "*.py" "*.ino" 
	DEPENDS ${LIB_SOURCES}
)

set(BOARD1 "arduino:avr:uno")
set(BOARD2 "esp8266:esp8266:nodemcuv2:FlashSize=4M3M,CpuFrequency=80")
add_custom_target(verify-examples
	COMMAND mkdir -p "${ARDUINO_LIBS}"
	COMMAND rm -f "${ARDUINO_LIBS}/Unisparks"
	COMMAND ln -s "${CMAKE_SOURCE_DIR}" "${ARDUINO_LIBS}/Unisparks"
	COMMAND echo === Ring/${BOARD1} ===
	COMMAND ${ARDUINO} --verify --board ${BOARD1} ${CMAKE_SOURCE_DIR}/examples/Ring/Ring.ino}
	#COMMAND echo === Vest/${BOARD1} ===
	#COMMAND ${ARDUINO} --verify --board ${BOARD1} ${CMAKE_SOURCE_DIR}/examples/Vest/Vest.ino}
	COMMAND echo === Ring/${BOARD2} ===
	COMMAND ${ARDUINO} --verify --board ${BOARD2} ${CMAKE_SOURCE_DIR}/examples/Ring/Ring.ino}
	COMMAND echo === Vest/${BOARD2} ===
	COMMAND ${ARDUINO} --verify --board ${BOARD2} ${CMAKE_SOURCE_DIR}/examples/Vest/Vest.ino}
	)

# PC LIBRARY
add_library(unisparks SHARED ${LIB_SOURCES})
target_include_directories(unisparks PUBLIC src)

# DEMO
add_executable(unisparks-demo EXCLUDE_FROM_ALL ${DEMO_SOURCES})
find_package(glfw3 3.2)
target_link_libraries(unisparks-demo unisparks glfw ${DEMO_SYSTEM_LIBS})
target_include_directories(unisparks-demo PRIVATE src)
add_custom_target(demo
	COMMAND unisparks-demo
    DEPENDS unisparks-demo)

# INSTALL
install(TARGETS unisparks
	LIBRARY DESTINATION lib
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/unisparks/
		DESTINATION include/unisparks
        FILES_MATCHING PATTERN "*.h"
)

# TESTS
add_custom_target(test DEPENDS unisparks unisparks-demo verify-examples)
add_custom_target(docker-test COMMAND docker run -v ${CMAKE_SOURCE_DIR}:/unisparks unisparks/ubuntu)

# Push Docker images (requires docker login)
add_custom_target(push-docker-image
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/extras
	COMMAND docker build -t unisparks/ubuntu .
	COMMAND docker run -v ${CMAKE_SOURCE_DIR}:/unisparks unisparks/ubuntu
	COMMAND docker push unisparks/ubuntu
	)


