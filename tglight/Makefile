.PHONY: all clean debug release-armv7 run docker-build docker-run docker-deploy
SOURCE_DIR:=${CURDIR}
BUILD_DIR:=${CURDIR}/build
CARGO:=cargo
DOCKER=docker

all: debug

run:
	${CARGO} run -- --config=${SOURCE_DIR}/etc/robot/tglight.toml ${TGLIGHT_FLAGS}

run-release:
	${CARGO} run --release -- --config=${SOURCE_DIR}/etc/robot/tglight.toml ${TGLIGHT_FLAGS}

clean:
	${CARGO} clean

debug:
	${CARGO} clean -p tglight
	${CARGO} build

release:
	${CARGO} clean -p tglight --release
	${CARGO} build --release

release-armv7:
	${CARGO} clean -p tglight --release
	${CARGO} build --target=armv7-unknown-linux-gnueabihf --release

install:
	mount -o remount,rw /
	${CARGO} clean -p tglight --release
	${CARGO} build --release
	service tglight stop
	mkdir -p /opt/tglight/bin
	cp ${BUILD_DIR}/release/tglight /opt/tglight/bin/tglight
	mkdir -p /opt/tglight/etc
	cp ${SOURCE_DIR}/etc/robot/tglight.toml /opt/tglight/etc/tglight.toml
	service tglight restart

setup:
	@if [ -z ${TGLIGHT_HOST} ]; then echo "Please set TGLIGHT_HOST environment variable" && exit 255; fi
	cat ${HOME}/.ssh/id_rsa.pub | ssh pi@${TGLIGHT_HOST} 'sudo mount -o remount,rw / && mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys'
	ssh pi@${TGLIGHT_HOST} 'sudo mkdir -p /root/.ssh && sudo cp /home/pi/.ssh/authorized_keys /root/.ssh/authorized_keys'

deploy: release-armv7
	@if [ -z ${TGLIGHT_HOST} ]; then echo "Please set TGLIGHT_HOST environment variable" && exit 255; fi
	@if [ -z ${CARGO_TARGET_DIR} ]; then echo "Please set CARGO_TARGET_DIR environment variable" && exit -1; fi
	-ssh root@${TGLIGHT_HOST} "service tglight stop"
	ssh pi@${TGLIGHT_HOST} "sudo mount -o remount,rw /"
	ssh root@${TGLIGHT_HOST} "mkdir -p /opt/tglight/bin"
	scp ${CARGO_TARGET_DIR}/armv7-unknown-linux-gnueabihf/release/tglight root@${TGLIGHT_HOST}:/opt/tglight/bin/tglight
	ssh root@${TGLIGHT_HOST} "service tglight restart"
	-ssh root@${TGLIGHT_HOST} "sudo reboot"

docker-image:
	${DOCKER} build -t tglight .

docker-run: docker-image
	${DOCKER} run -it --rm -p 8080:8080 -v ${BUILD_DIR}/docker:/build/tglight tglight

docker-deploy: docker-image
	@if [ -z ${TGLIGHT_HOST} ]; then echo "Please set TGLIGHT_HOST environment variable" && exit 255; fi
	${DOCKER} run -it --rm  -v ${BUILD_DIR}/docker:/build/tglight -e "TGLIGHT_HOST=${TGLIGHT_HOST}" tglight deploy

docker-shell: docker-image
	${DOCKER} run -it --rm -v${SOURCE_DIR}:/workdir --entrypoint=/bin/bash tglight

copy-pp-cards:
	- cp etc/robot/pixel.rc /Volumes/PP_ROBOT/pixel.rc && ls -la /Volumes/PP_ROBOT/
	- cp etc/stage/pixel.rc /Volumes/PP_STAGE/pixel.rc && ls -la /Volumes/PP_STAGE/
	- cp etc/caboose/pixel.rc /Volumes/PP_CABOOSE/pixel.rc && ls -la /Volumes/PP_CABOOSE/
