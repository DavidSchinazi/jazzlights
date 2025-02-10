# The goal of this docker file is to allow testing our networking code on Linux locally from macOS.

# docker build . -t jazzlights/linux && docker run --rm --network=host -it jazzlights/linux

FROM debian:stable-slim

# Install pre-built dependencies.
RUN apt update && apt install -y cmake clang libwayland-dev libxkbcommon-dev xorg-dev curl unzip

# Install latest GLFW release from source.
RUN <<EOF
    GLFW_VERSION=$(curl -Ls https://api.github.com/repos/glfw/glfw/releases/latest | grep tag_name | sed 's/.*:[^"]"\([^"]*\)".*/\1/')
    echo ${GLFW_VERSION}
    curl -OLs https://github.com/glfw/glfw/releases/download/${GLFW_VERSION}/glfw-${GLFW_VERSION}.zip
    rm -rf glfw-${GLFW_VERSION} >/dev/null 2>&1
    unzip -q glfw-${GLFW_VERSION}.zip
    cmake -S glfw-${GLFW_VERSION} -B glfw-${GLFW_VERSION}/build
    make -C glfw-${GLFW_VERSION}/build install
EOF

# Copy jazzlights source to container.
COPY . /jl

# Prepare build files.
RUN cmake -S /jl/extras -B /build-docker

# Compile jazzlights.
RUN cmake --build /build-docker

# Run the headless benchmark tool with networking enabled.
CMD /build-docker/jazzlights-bench -n
