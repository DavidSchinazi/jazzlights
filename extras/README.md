# Extras

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
    cd jazzlights/extras
    cmake -S . -B build
    cmake --build build
```

# Run

```
    jazzlights/extras/build/jazzlights-demo
    jazzlights/extras/build/jazzlights-bench
```
