# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

# Our sources need mbedTLS's public compile definitions, otherwise they silently use its default configuration instead
# of ESP-IDF's, which has a different ABI.
idf_component_register(SRCS ${app_sources} REQUIRES mbedtls)
