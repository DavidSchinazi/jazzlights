# Unisparks Extras

These are tools that can run on your computer.

# Setup Dependencies

First you'll need to build and install [GLFW3](https://www.glfw.org/).
Compilation instructions are [here](https://www.glfw.org/docs/latest/compile.html).

Summary for macOS:

```
    cd GLFW_DIR
    cmake -S . -B build
    sudo make -C build install
```

# Build

```
    cd unisparks/extras
    cmake -S . -B build
    cmake --build build
```

# Run

```
    unisparks/extras/build/unisparks-demo unisparks/extras/demo/matrix.toml
    unisparks/extras/build/unisparks-bench
```
