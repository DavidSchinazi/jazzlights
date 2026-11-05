# Builds the secure channel server and OTA handler natively, so that extras/scripts/test_secure_channel.py can test them
# without hardware. The headers in shim/ stand in for ESP-IDF, FreeRTOS and lwIP, and fake the flash.
cmake_minimum_required(VERSION 3.16)
project(SecureChannelTest C CXX)

include(FetchContent)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
# This is the version of mbedTLS that's in ESP-IDF.
FetchContent_Declare(
  mbedtls
  URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.2/mbedtls-3.6.2.tar.bz2
  URL_HASH SHA256=8b54fb9bcf4d5a7078028e0520acddefb7900b3e66fec7f7175bb5b7d85ccdca
)
FetchContent_MakeAvailable(mbedtls)

add_executable(secure-channel-test-server
  main.cpp
  ../../src/jazzlights/network/ota_handler.cpp
  ../../src/jazzlights/network/secure_channel_server.cpp
  ../../src/jazzlights/network/tlv.cpp
  ../../src/jazzlights/util/log.cpp
  ../../src/jazzlights/util/time.cpp
)
target_include_directories(secure-channel-test-server PRIVATE shim ../../src)
target_compile_features(secure-channel-test-server PRIVATE cxx_std_20)
target_compile_definitions(secure-channel-test-server PRIVATE
  JL_CONFIG=NONE JL_CONTROLLER=NATIVE JL_SECURE_CHANNEL=1 JL_OTA=1 JL_SECURE_CHANNEL_ECHO=1)
target_compile_options(secure-channel-test-server PRIVATE
  -Wall -Wextra -g -fsanitize=address,undefined -fno-sanitize-recover=all)
target_link_options(secure-channel-test-server PRIVATE -fsanitize=address,undefined)
find_package(Threads REQUIRED)
target_link_libraries(secure-channel-test-server PRIVATE MbedTLS::mbedcrypto Threads::Threads)
