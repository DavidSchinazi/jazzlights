cmake_minimum_required(VERSION 3.9)
set(CMAKE_MACOSX_RPATH 1)
project(Jazzlights)

set(LIB_SOURCES
	../src/jazzlights/effect.hpp
	../src/jazzlights/effects/flame.cpp
	../src/jazzlights/effects/flame.hpp
	../src/jazzlights/effects/functional.hpp
	../src/jazzlights/effects/glitter.hpp
	../src/jazzlights/effects/glow.hpp
	../src/jazzlights/effects/plasma.hpp
	../src/jazzlights/effects/rainbow.hpp
	../src/jazzlights/effects/solid.hpp
	../src/jazzlights/effects/threesine.hpp
	../src/jazzlights/frame.hpp
	../src/jazzlights/layout.hpp
	../src/jazzlights/layouts/matrix.hpp
	../src/jazzlights/layouts/pixelmap.hpp
	../src/jazzlights/layouts/reversemap.hpp
	../src/jazzlights/network.cpp
	../src/jazzlights/network.hpp
	../src/jazzlights/player.cpp
	../src/jazzlights/player.hpp
	../src/jazzlights/pseudorandom.cpp
	../src/jazzlights/pseudorandom.h
	../src/jazzlights/registry.cpp
	../src/jazzlights/registry.hpp
	../src/jazzlights/renderer.hpp
	../src/jazzlights/renderers/simple.hpp
	../src/jazzlights/types.cpp
	../src/jazzlights/types.h
	../src/jazzlights/util/color.cpp
	../src/jazzlights/util/color.hpp
	../src/jazzlights/util/geom.hpp
	../src/jazzlights/util/log.cpp
	../src/jazzlights/util/log.hpp
	../src/jazzlights/util/math.cpp
	../src/jazzlights/util/math.hpp
	../src/jazzlights/util/memory.hpp
	../src/jazzlights/util/meta.hpp
	../src/jazzlights/util/noise.cpp
	../src/jazzlights/util/noise.h
	../src/jazzlights/util/stream.hpp
	../src/jazzlights/util/time.hpp
	../src/jazzlights/version.hpp
	jazzlights/effects/pixeltest.hpp
	jazzlights/networks/udp.cpp
	jazzlights/networks/udp.hpp
	jazzlights/util/cpptoml.hpp
	jazzlights/util/loader.cpp
	jazzlights/util/loader.hpp
	jazzlights/util/stdlog.cpp
	jazzlights/util/stdtime.cpp
)

set(DEMO_SOURCES
	demo/glrenderer.cpp
	demo/glrenderer.hpp
	demo/gui.cpp
	demo/gui.hpp
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
