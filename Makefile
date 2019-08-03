.PHONY: all clean install install-local demo test makeproj xcodeproj verify-examples
SOURCE_DIR:=${CURDIR}
BUILD_DIR:=${CURDIR}/build
CMAKE:=${if ${CMAKE},${CMAKE},cmake}
CMAKE_MAKEPROJ_FLAGS:=\
	-D CMAKE_RUNTIME_OUTPUT_DIRECTORY=${BUILD_DIR}/bin \
	-D CMAKE_ARCHIVE_OUTPUT_DIRECTORY=${BUILD_DIR}/lib \
	-D CMAKE_LIBRARY_OUTPUT_DIRECTORY=${BUILD_DIR}/lib
CMAKE_XCODEPROJ_FLAGS:=\
	-D CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG=${BUILD_DIR}/bin \
	-D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG=${BUILD_DIR}/lib \
	-D CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG=${BUILD_DIR}/lib \
	-D CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE=${BUILD_DIR}/bin \
	-D CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE=${BUILD_DIR}/lib \
	-D CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE=${BUILD_DIR}/lib
CMAKE_BUILD_DIR:=${BUILD_DIR}/tmp
ARDUINO:=${if ${ARDUINO},${ARDUINO},arduino}
ARDUINO_BOARD:=${if ${ARDUINO_BOARD},${ARDUINO_BOARD},"esp8266:esp8266:huzzah:eesz=4M3M,xtal=80"}


all: ${CMAKE_BUILD_DIR}
	${CMAKE} --build ${CMAKE_BUILD_DIR}

clean: 
	${CMAKE} -E remove_directory ${CURDIR}/build

install: all
	${CMAKE} -P ${CMAKE_BUILD_DIR}/cmake_install.cmake 

demo: ${CMAKE_BUILD_DIR}
	${CMAKE} --build ${CMAKE_BUILD_DIR} --config release --target unisparks-demo 
	LD_LIBRARY_PATH=${BUILD_DIR}/lib \
		${BUILD_DIR}/bin/unisparks-demo ${SOURCE_DIR}/extras/demo/matrix.toml

bench: ${CMAKE_BUILD_DIR}
	${CMAKE} --build ${CMAKE_BUILD_DIR} --config release --target unisparks-bench
	LD_LIBRARY_PATH=${BUILD_DIR}/lib ${BUILD_DIR}/bin/unisparks-bench

test: ${CMAKE_BUILD_DIR}
	${CMAKE} --build ${CMAKE_BUILD_DIR} --config debug --target unisparks-test
	LD_LIBRARY_PATH=${BUILD_DIR}/lib ${BUILD_DIR}/bin/unisparks-test

xcodeproj: clean
	${CMAKE} -E make_directory ${CMAKE_BUILD_DIR}
	cd ${CMAKE_BUILD_DIR} && ${CMAKE} -G Xcode ${CMAKE_XCODEPROJ_FLAGS} ${SOURCE_DIR}/extras

makeproj: clean ${CMAKE_BUILD_DIR}

verify-examples:
	${ARDUINO} --verify --board ${ARDUINO_BOARD} examples/Ring/Ring.ino

${CMAKE_BUILD_DIR}:
	${CMAKE} -E make_directory $@
	cd $@ && ${CMAKE} ${CMAKE_MAKEPROJ_FLAGS} ${SOURCE_DIR}/extras
