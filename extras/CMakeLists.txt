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

set(JLCompileOptions "-std=c++20;-DJL_CONFIG=NONE;-DJL_CONTROLLER=NATIVE;-fPIC;-Wall;-Wextra;-Werror;-Wno-deprecated-declarations")

# LIBRARY
add_library(jazzlights SHARED ${LIB_SOURCES})
target_include_directories(jazzlights PUBLIC ../src .)
set_target_properties(jazzlights PROPERTIES COMPILE_OPTIONS "${JLCompileOptions}")
install(TARGETS jazzlights LIBRARY DESTINATION lib)
install(DIRECTORY src/ COMPONENT "lib" DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY extras/jazzlights/ COMPONENT "lib" DESTINATION include/jazzlights FILES_MATCHING PATTERN "*.h")

# DEMO
find_package(glfw3 3.2 REQUIRED)
find_package(OpenGL REQUIRED)
add_executable(jazzlights-demo ${DEMO_SOURCES})
target_link_libraries(jazzlights-demo jazzlights glfw ${OPENGL_gl_LIBRARY})
target_include_directories(jazzlights-demo PRIVATE ../src ${GLFW_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR})
set_target_properties(jazzlights-demo PROPERTIES COMPILE_OPTIONS "${JLCompileOptions}")
install(TARGETS jazzlights-demo DESTINATION bin COMPONENT "demo")

# DEMO-ASAN
find_package(glfw3 3.2 REQUIRED)
find_package(OpenGL REQUIRED)
add_executable(jazzlights-demo-asan ${DEMO_SOURCES})
target_link_libraries(jazzlights-demo-asan jazzlights glfw ${OPENGL_gl_LIBRARY})
target_include_directories(jazzlights-demo-asan PRIVATE ../src ${GLFW_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR})
set_target_properties(jazzlights-demo-asan PROPERTIES COMPILE_OPTIONS "${JLCompileOptions};-fsanitize=address")
set_target_properties(jazzlights-demo-asan PROPERTIES LINK_OPTIONS "-fsanitize=address")
install(TARGETS jazzlights-demo-asan DESTINATION bin COMPONENT "demo")

# BENCH
add_executable(jazzlights-bench ${BENCH_SOURCES})
target_link_libraries(jazzlights-bench jazzlights)
set_target_properties(jazzlights-bench PROPERTIES COMPILE_OPTIONS "${JLCompileOptions}")
install(TARGETS jazzlights-bench DESTINATION bin COMPONENT "bench")
