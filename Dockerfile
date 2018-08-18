FROM ubuntu:18.04

# Set up development environment
RUN apt-get update && apt-get install -y\
	apt-utils\
	git\
	build-essential\
	curl

# Rust
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y

# Raspberry Pi toolchain
RUN apt-get install -y\
		clang\
		libclang-dev\
		gcc-arm-linux-gnueabihf\
		g++-arm-linux-gnueabihf &&\
	/root/.cargo/bin/rustup target add armv7-unknown-linux-gnueabihf
RUN printf "\n[target.armv7-unknown-linux-gnueabihf]\nlinker = \"arm-linux-gnueabihf-g++\"\nar = \"arm-linux-gnueabihf-ar\"\n" >> /root/.cargo/config

# Common env
ENV PATH="/root/.cargo/bin:${PATH}"
ENV USER="root"

# Add deployment keys and configure ssh
ADD etc/tglight_deploy_id_rsa /root/.ssh/id_rsa
ADD etc/tglight_deploy_id_rsa.pub /root/.ssh/id_rsa.pub
RUN chmod 700 /root/.ssh &&\
	chmod 700 /root/.ssh/id_rsa &&\
	printf "    IdentityFile ~/.ssh/id_rsa\n    StrictHostKeyChecking no\n" >> /etc/ssh/ssh_config

RUN mkdir /src

# Download unisparks
RUN git clone https://github.com/unisparks/unisparks.git /src/unisparks -b dev
ENV UNISPARKS_DIR=/src/unisparks

# Download and compile tglight
RUN git clone git@gitlab.com:technogecko/tglight.git  /src/tglight
WORKDIR /src/tglight
ENV CARGO_TARGET_DIR=/build/tglight
RUN make debug release-armv7

# Set up entry point
ADD . /workdir
WORKDIR /workdir
ENTRYPOINT ["/usr/bin/make"]
CMD ["run"]

