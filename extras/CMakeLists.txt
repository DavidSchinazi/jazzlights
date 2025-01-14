cmake_minimum_required(VERSION 3.16)
set(CMAKE_MACOSX_RPATH 1)
project(Jazzlights)

file(GLOB_RECURSE LIB_SOURCES
	../src/jazzlights/*.cpp
	../src/jazzlights/*.h
)

file(GLOB_RECURSE DEMO_SOURCES
	demo/*.cpp
	demo/*.h
)

file(GLOB_RECURSE BENCH_SOURCES
	bench/*.cpp
	bench/*.h
)

find_package(glfw3 3.2 REQUIRED)
find_package(OpenGL REQUIRED)

set(JLCompileOptions
	"-std=c++20;-DJL_CONFIG=NONE;-DJL_CONTROLLER=NATIVE;-fPIC;-Wall;-Wextra;-Werror;-Wno-deprecated-declarations"
)

# LIBRARY
add_library(jazzlights SHARED ${LIB_SOURCES})
target_include_directories(jazzlights PUBLIC ../src .)
set_target_properties(jazzlights PROPERTIES COMPILE_OPTIONS "${JLCompileOptions}")

# LIBRARY-ASAN
add_library(jazzlights-asan SHARED ${LIB_SOURCES})
target_include_directories(jazzlights-asan PUBLIC ../src .)
set_target_properties(jazzlights-asan PROPERTIES COMPILE_OPTIONS "${JLCompileOptions};-fsanitize=address;-g")
set_target_properties(jazzlights-asan PROPERTIES LINK_OPTIONS "-fsanitize=address")

# DEMO-LIB
add_library(jazzlights-demo-lib INTERFACE)
target_link_libraries(jazzlights-demo-lib INTERFACE glfw OpenGL::GL)
target_include_directories(jazzlights-demo-lib INTERFACE ../src ${GLFW_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR})

# DEMO
add_executable(jazzlights-demo ${DEMO_SOURCES})
target_link_libraries(jazzlights-demo PRIVATE jazzlights jazzlights-demo-lib)
set_target_properties(jazzlights-demo PROPERTIES COMPILE_OPTIONS "${JLCompileOptions}")

# DEMO-ASAN
add_executable(jazzlights-demo-asan ${DEMO_SOURCES})
target_link_libraries(jazzlights-demo-asan PRIVATE jazzlights-asan jazzlights-demo-lib)
set_target_properties(jazzlights-demo-asan PROPERTIES COMPILE_OPTIONS "${JLCompileOptions};-fsanitize=address;-g")
set_target_properties(jazzlights-demo-asan PROPERTIES LINK_OPTIONS "-fsanitize=address")

# BENCH
add_executable(jazzlights-bench ${BENCH_SOURCES})
target_link_libraries(jazzlights-bench jazzlights)
set_target_properties(jazzlights-bench PROPERTIES COMPILE_OPTIONS "${JLCompileOptions}")
