# Extras

These are tools that can run on your computer.

# Setup Dependencies

First you'll need to build and install [GLFW3](https://www.glfw.org/) using the steps below.
If in doubt, compilation instructions are [here](https://www.glfw.org/docs/latest/compile.html).

```
cd /tmp
GLFW_VERSION=$(curl -Ls https://api.github.com/repos/glfw/glfw/releases/latest | grep tag_name | sed 's/.*:[^"]"\([^"]*\)".*/\1/')
echo ${GLFW_VERSION}
curl -OLs https://github.com/glfw/glfw/releases/download/${GLFW_VERSION}/glfw-${GLFW_VERSION}.zip
rm -rf glfw-${GLFW_VERSION} >/dev/null 2>&1
unzip -q glfw-${GLFW_VERSION}.zip
cmake -S glfw-${GLFW_VERSION} -B glfw-${GLFW_VERSION}/build
sudo make -C glfw-${GLFW_VERSION}/build install
```

# Build

```
cd jazzlights/extras
cmake -S . -B build
cmake --build build
```

# Run

```
jazzlights/extras/build/jazzlights-demo
jazzlights/extras/build/jazzlights-demo-asan
jazzlights/extras/build/jazzlights-bench
```
