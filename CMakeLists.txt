cmake_minimum_required(VERSION 2.8)
project(dfsparks)

set(ARDUINOLIB_SOURCES 
	src/arduinolib/DFSParks/DFSParks.h 
	src/arduinolib/DFSParks/DFSParks_Color.h
	src/arduinolib/DFSParks/DFSParks_Color.cpp
	src/arduinolib/DFSParks/DFSParks_Protocol.h
	src/arduinolib/DFSParks/DFSParks_Protocol.cpp
	src/arduinolib/DFSParks/DFSParks_Strand.h
	src/arduinolib/DFSParks/DFSParks_Strand.cpp)

set(DEMO_SOURCES 
	src/demo/client.h
	src/demo/client.cpp
	src/demo/server.h
	src/demo/server.cpp
	src/demo/socket.h
	src/demo/socket.cpp
	src/demo/task.h
	src/demo/ui.h
	src/demo/ui.cpp
	src/demo/main.cpp)

set(TEST_SOURCES 
	tests/catch.hpp
	tests/test_color.cpp
	tests/test_protocol.cpp
	tests/test_main.cpp)

set(CMAKE_CXX_FLAGS "-std=c++11 -DVERSION=\"0.1\"")

# ARDUINO LIBRARY
add_custom_target(arduinolib ALL DEPENDS ${CMAKE_BINARY_DIR}/DFSparks.zip)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/DFSparks.zip 
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src/arduinolib
	COMMAND echo Building ${CMAKE_BINARY_DIR}/DFSparks.zip
	COMMAND zip -r ${CMAKE_BINARY_DIR}/DFSparks.zip DFSparks
	#DEPENDS ${ARDUINOLIB_SOURCES}
)

# PC LIBRARY
add_library(dfsparks STATIC ${ARDUINOLIB_SOURCES})
target_include_directories(dfsparks PUBLIC src/arduinolib/DFSparks)

# DEMO
add_executable(dfsparks_demo ${DEMO_SOURCES})
target_link_libraries(dfsparks_demo dfsparks glfw3)
target_include_directories(dfsparks_demo PRIVATE src)

# TEST
add_executable(dfsparks_test ${TEST_SOURCES})
target_link_libraries(dfsparks_test dfsparks)
add_custom_command(TARGET dfsparks_test POST_BUILD COMMAND dfsparks_test)

add_test(dfsparks_test dfsparks_test)

# PLATFORM DETAILS
if (APPLE)
	include_directories(/usr/local/include)
	set(ARDUINOLIB_INSTALL_PATH /Users/azov/Documents/Arduino/libraries)
	target_link_libraries(dfsparks_demo "-L/usr/local/lib -framework Cocoa -framework OpenGL -framework IOKit -framework CoreFoundation -framework CoreVideo")
endif()

# INSTALL
if (DEFINED ARDUINOLIB_INSTALL_PATH)
	install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/arduinolib/DFSparks/ 
		DESTINATION ${ARDUINOLIB_INSTALL_PATH}/DFSparks/)
endif()
