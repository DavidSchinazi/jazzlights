# JazzLights

JazzLights is LED software focused on syncing patterns between devices. It's
designed to work in the absence of network infrastructure, such as in the Black
Rock Desert. Devices can use Wi-Fi and Bluetooth Low Energy to communicate over
an unconnected mesh network that automatically elects and follows a leader.
Devices will stay in sync over time as long as a packet gets through once in a
while to avoid clock skew.

JazzLights was the basis for large art cars and dozens of individual vests and
other accessories. It's worked well out in the desert for several years.
Instructions for building your own vest are [here](extras/docs/VEST.md).
JazzLights is also integrated with
[Home Assistant](https://github.com/DavidSchinazi/jazzlights/tree/main/extras/home-assistant).

As such, JazzLights is not designed as a general-purpose LED library suitable
for all projects. Adding new patterns or device layouts requires modifying the
C++ code. For cases where syncing is not required or the networking conditions
are less challenging, [WLED](https://kno.wled.ge/) might be a better
alternative.

JazzLights supports various models and variants of ESP32 and focuses on boards
made by [M5Stack](https://m5stack.com/), in particular the
[ATOM Matrix](https://shop.m5stack.com/products/atom-matrix-esp32-development-kit)
and [ATOM S3](https://shop.m5stack.com/products/atoms3-dev-kit-w-0-85-inch-screen).
JazzLights is developed in C++ and built using
[PlatformIO](https://platformio.org/). It leverages [FastLED](https://fastled.io/)
and can work with any LEDs supported by FastLED.

## Installation

The most common build targets can be installed directly from your Web browser
to an ESP32 using
[Jazzlight's Web Installer](https://davidschinazi.github.io/jazzlights/flash/).
Otherwise, all other targets can be installed using PlatformIO.

First install PlatformIO for your favorite editor
[here](https://platformio.org/platformio-ide). If you're not sure which editor
to pick, we recommend VSCode (Visual Studio Code). That'll also install the
`pio` command-line tool. If you've installed PlatformIO more than a few weeks
ago, make sure to update your tools by running: `pio pkg update`.

To install JazzLights for a furry vest on an M5Stack ATOM Matrix, you can run:
`pio run -e vest -t upload`.

<!-- website-skip-start -->

## Development and Debugging

See [DEBUGGING.md](DEBUGGING.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

## Acknowledgements

This project initially started off as a fork of
[unisparks](https://github.com/unisparks/unisparks)
by Dmitry Azovtsev and Igor Chernyshev.
