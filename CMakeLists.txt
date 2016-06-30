cmake_minimum_required(VERSION 2.8)
project(DFSparks)
set(CMAKE_MACOSX_RPATH 1)

set(LIB_SOURCES 
	src/DFSparks/DFSparks.h 
	src/DFSparks/DFSparks_Color.cpp
	src/DFSparks/DFSparks_Color.h
	src/DFSparks/DFSparks_Effect.h
	src/DFSparks/DFSparks_Effect_Blink.h
	src/DFSparks/DFSparks_Effect_Flame.h
	src/DFSparks/DFSparks_Effect_Flame.cpp
	src/DFSparks/DFSparks_Effect_Rainbow.h
	src/DFSparks/DFSparks_Effects.h
	src/DFSparks/DFSparks_Effects.cpp
	src/DFSparks/DFSparks_Log.h
	src/DFSparks/DFSparks_Log.cpp
	src/DFSparks/DFSparks_Math.h
	src/DFSparks/DFSparks_Math.cpp
	src/DFSparks/DFSparks_Network.h
	src/DFSparks/DFSparks_Network.cpp
	src/DFSparks/DFSparks_Palette.cpp
	src/DFSparks/DFSparks_Palette.h
	src/DFSparks/DFSparks_Player.h
	src/DFSparks/DFSparks_Timer.cpp
	src/DFSparks/DFSparks_Timer.h
)

set(DEMO_SOURCES 
	src/Demo/ui.h
	src/Demo/ui.cpp
	src/Demo/main.cpp)

set(CMAKE_CXX_FLAGS "-fPIC -std=c++11 -Wall -DVERSION=\"0.1\"")

# ARDUINO LIBRARY
add_custom_target(arduinolib ALL DEPENDS ${CMAKE_BINARY_DIR}/DFSparks.zip)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/DFSparks.zip 
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
	COMMAND echo Building ${CMAKE_BINARY_DIR}/DFSparks.zip
	COMMAND zip -r ${CMAKE_BINARY_DIR}/DFSparks.zip DFSparks --include "*.cpp" "*.h" "*.py" "*.ino"
	DEPENDS ${LIB_SOURCES}
)

# PC LIBRARY
add_library(dfsparks STATIC ${LIB_SOURCES})
target_include_directories(dfsparks 
	PUBLIC src/DFSparks)

# DEMO
add_executable(dfsparks_demo ${DEMO_SOURCES})
target_link_libraries(dfsparks_demo dfsparks glfw3)
target_include_directories(dfsparks_demo PRIVATE src)

# PLATFORM DETAILS
if (APPLE)
	include_directories(/usr/local/include)
	set(ARDUINOLIB_INSTALL_PATH /Users/azov/Documents/Arduino/libraries)
	target_link_libraries(dfsparks_demo "-L/usr/local/lib -framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo")
endif()

# INSTALL
if (DEFINED ARDUINOLIB_INSTALL_PATH)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/DFSparks/ 
		DESTINATION ${ARDUINOLIB_INSTALL_PATH}/DFSparks/)
endif()
