cmake_minimum_required(VERSION 2.8)
project(dfsparks)
set(CMAKE_MACOSX_RPATH 1)

set(LIB_SOURCES 
	src/DFSParks/DFSParks.h 
	src/DFSParks/DFSParks_Color.cpp
	src/DFSParks/DFSParks_Color.h
	src/DFSParks/DFSParks_Effect.h
	src/DFSParks/DFSParks_Effect_Blink.h
	src/DFSParks/DFSParks_Effect_Flame.h
	src/DFSParks/DFSParks_Effect_Flame.cpp
	src/DFSParks/DFSParks_Effect_Colorfill.h
	src/DFSParks/DFSParks_Effect_Rainbow.h
	src/DFSParks/DFSParks_Effects.h
	src/DFSParks/DFSParks_Effects.cpp
	src/DFSParks/DFSParks_Log.h
	src/DFSParks/DFSParks_Log.cpp
	src/DFSParks/DFSParks_Math.h
	src/DFSParks/DFSParks_Math.cpp
	src/DFSParks/DFSParks_Network.h
	src/DFSParks/DFSParks_Network.cpp
	src/DFSParks/DFSParks_Palette.cpp
	src/DFSParks/DFSParks_Palette.h
	src/DFSParks/DFSParks_Player.h
	src/DFSParks/DFSParks_Timer.cpp
	src/DFSParks/DFSParks_Timer.h
)

set(DEMO_SOURCES 
	src/Demo/ui.h
	src/Demo/ui.cpp
	src/Demo/main.cpp)

set(CMAKE_CXX_FLAGS "-std=c++14 -DVERSION=\"0.1\"")

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
