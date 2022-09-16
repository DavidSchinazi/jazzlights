# JazzLights Library

This library contains the LED software that runs on
[TechnoGecko](https://www.technogecko.org/) vehicles and furry LED vests.
The Arduino software is C++ built using [PlatformIO](https://platformio.org/).
The Raspberry Pi version for vehicles currently uses Rust,
though we are planning on switching those to Arduino.

## Installation on Arduino for vests

First install PlatformIO for your favorite editor [here](https://platformio.org/platformio-ide).
If you're not sure which editor to pick, we recommend VSCode (Visual Studio Code).
That'll also install the `pio` command-line tool.

If you've installed PlatformIO more than a few weeks ago, make sure to update your tools by running `pio pkg update`.

To install the software on an M5Stack ATOM Matrix
([available for purchase here](https://shop.m5stack.com/collections/m5-atom/products/atom-matrix-esp32-development-kit)),
you can run: `pio run -e atom_matrix -t upload`

This project focuses on the M5Stack ATOM Matrix but other ESP32 and ESP8266 boards are also supported.

## Installation on Raspberry Pi for vehicles

First [install the rust/cargo toolchain](https://www.rust-lang.org/tools/install),
then from the `jazzlights/tglight` directory you can run
`cargo run -- --timestamp --config ./etc/tglight-from-robot.toml`

## Debugging

See [DEBUGGING.md](DEBUGGING.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Acknowledgements

This project initially started off as a fork of
[unisparks](https://github.com/unisparks/unisparks) by Dmitry Azovtsev and Igor Chernyshev.
