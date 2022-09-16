cmake_minimum_required(VERSION 3.9)
set(CMAKE_MACOSX_RPATH 1)
project(Jazzlights)

set(LIB_SOURCES
	../src/jazzlights/effect.h
	../src/jazzlights/effects/flame.cpp
	../src/jazzlights/effects/flame.h
	../src/jazzlights/effects/functional.h
	../src/jazzlights/effects/glitter.h
	../src/jazzlights/effects/glow.h
	../src/jazzlights/effects/plasma.h
	../src/jazzlights/effects/rainbow.h
	../src/jazzlights/effects/solid.h
	../src/jazzlights/effects/threesine.h
	../src/jazzlights/frame.h
	../src/jazzlights/layout.h
	../src/jazzlights/layouts/matrix.h
	../src/jazzlights/layouts/pixelmap.h
	../src/jazzlights/layouts/reversemap.h
	../src/jazzlights/network.cpp
	../src/jazzlights/network.h
	../src/jazzlights/player.cpp
	../src/jazzlights/player.h
	../src/jazzlights/pseudorandom.cpp
	../src/jazzlights/pseudorandom.h
	../src/jazzlights/registry.cpp
	../src/jazzlights/registry.h
	../src/jazzlights/renderer.h
	../src/jazzlights/renderers/simple.h
	../src/jazzlights/types.cpp
	../src/jazzlights/types.h
	../src/jazzlights/util/color.cpp
	../src/jazzlights/util/color.h
	../src/jazzlights/util/geom.h
	../src/jazzlights/util/log.cpp
	../src/jazzlights/util/log.h
	../src/jazzlights/util/math.cpp
	../src/jazzlights/util/math.h
	../src/jazzlights/util/memory.h
	../src/jazzlights/util/meta.h
	../src/jazzlights/util/noise.cpp
	../src/jazzlights/util/noise.h
	../src/jazzlights/util/stream.h
	../src/jazzlights/util/time.h
	../src/jazzlights/version.h
	jazzlights/effects/pixeltest.h
	jazzlights/networks/udp.cpp
	jazzlights/networks/udp.h
	jazzlights/util/cpptoml.h
	jazzlights/util/loader.cpp
	jazzlights/util/loader.h
	jazzlights/util/stdlog.cpp
	jazzlights/util/stdtime.cpp
)

set(DEMO_SOURCES
	demo/glrenderer.cpp
	demo/glrenderer.h
	demo/gui.cpp
	demo/gui.h
	demo/main.cpp
)

set(BENCH_SOURCES
	bench/main.cpp
)

# LIBRARY
find_package(glfw3 3.2 REQUIRED)
find_package(OpenGL REQUIRED)
add_library(jazzlights SHARED ${LIB_SOURCES})
target_include_directories(jazzlights PUBLIC ../src . PRIVATE ${GLFW_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR})
target_link_libraries(jazzlights glfw ${OPENGL_gl_LIBRARY})
set_target_properties(jazzlights PROPERTIES COMPILE_OPTIONS "-std=c++17;-fPIC;-Wall;-Wextra;-Werror;-Wno-deprecated-declarations")
install(TARGETS jazzlights LIBRARY DESTINATION lib)
install(DIRECTORY src/ COMPONENT "lib" DESTINATION include FILES_MATCHING PATTERN "*.h" PATTERN "Jazzlights.h" EXCLUDE)
install(DIRECTORY extras/jazzlights/ COMPONENT "lib" DESTINATION include/jazzlights FILES_MATCHING PATTERN "*.h")

# DEMO
add_executable(jazzlights-demo ${DEMO_SOURCES})
target_link_libraries(jazzlights-demo jazzlights)
target_include_directories(jazzlights-demo PRIVATE ../src)
set_target_properties(jazzlights-demo PROPERTIES COMPILE_OPTIONS "-std=c++17;-fPIC;-Wall;-Wextra;-Werror;-Wno-deprecated-declarations")
install(TARGETS jazzlights-demo DESTINATION bin COMPONENT "demo")

# BENCH
add_executable(jazzlights-bench ${BENCH_SOURCES})
target_link_libraries(jazzlights-bench jazzlights)
set_target_properties(jazzlights-bench PROPERTIES COMPILE_OPTIONS "-std=c++17;-fPIC;-Wall;-Wextra;-Werror;-Wno-deprecated-declarations")
install(TARGETS jazzlights-bench DESTINATION bin COMPONENT "bench")
