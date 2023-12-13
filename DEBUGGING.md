# To symbolicate a stack trace

First install the [ESP IDF tools](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html)

The in a terminal run this once: `. $HOME/esp/esp-idf/export.sh`
Then `xtensa-esp32-elf-addr2line` will be in the PATH and can be used as follows:

`xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/vest_dev/firmware.elf 0x400d34cc:0x3ffced20 0x400d33da:0x3ffced60 0x401c9779:0x3ffcee00 0x400d4276:0x3ffcee20 0x400d3c4b:0x3ffcee80 0x400d41d1:0x3ffceed0 0x400d424e:0x3ffcef90 0x400d5011:0x3ffcefb0 0x400d11fb:0x3ffcf000 0x400db62c:0x3ffcf020 0x40090a8e:0x3ffcf040`

Otherwise the location of `xtensa-esp32-elf-addr2line` can be found by running `find $HOME/.espressif/tools/xtensa-esp32-elf/ -name xtensa-esp32-elf-addr2line`

# To build from the command line

Run from `jazzlights`:
```
pio run -e vest_dev -t upload && pio device monitor -e vest_dev
```

# To build all available PlatformIO environments

This is useful to load all dependencies locally so you can build them later without Internet connectivity.

Run from `jazzlights`:
```
for e in $(pio project config | grep 'env:' | grep -v extends | sed 's/env://g') ; do if [ "${e:0:1}" != "_" ] ; then pio run -e "$e" || break ; fi ; done
```

# To copy this git directory over SSH from a laptop to a Raspberry Pi without Internet connectivity

First run once on your laptop:
```
git remote add pi pi:/root/jazzlights
```

Also remember to make sure the destination filesystem is readable!

Then to copy the local main branch over:
```
git push pi main:main
```

# To tweak the ESP-IDF settings for instrumentation

Run from `jazzlights`:
```
pio run -e vest_instrumentation -t menuconfig
```

# DS33 dual devices

```
pio run -e ds33_dev_a -t upload && pio device monitor -e ds33_dev_a
pio run -e ds33_dev_b -t upload && pio device monitor -e ds33_dev_b
```

# Instrumentation

The `vest_instrumentation` PlatformIO build config allows building a vest with a custom-built version
of FreeRTOS that has additional debugging / instrumentation information available.


Note that on macOS, this requires installing gawk from brew and putting it on PATH.
```
brew install gsed gawk jq
export PATH="/opt/homebrew/opt/gawk/libexec/gnubin:$PATH"
```

This requires installing the `esp-idf` tools:

```
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32
. ./export.sh
```

Additionally, this mode leverages the `sdkconfig.vest_instrumentation` file in this repository. That file was
originally generated manually but with hopes of updating it by pulling the default sdkconfig from
[here](https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/esp32/sdkconfig). However, that file was removed when
[arduino-esp32 updated to esp-idf v5.1](https://github.com/espressif/arduino-esp32/commit/6f7a1ca76ada4d705d79ce863450c91349327a79)'
(previous version was
[here](https://github.com/espressif/arduino-esp32/blob/082a61ebe476384941bf4e38ce0363f928adba12/tools/sdk/esp32/sdkconfig)).
In order to get the default sdkconfig, we now need to generate it ourselves. That can be done with the commands below.

First, determine which version of esp-idf is used by default. There should be a line like the following in the output of
`pio run`:

```
framework-espidf @ 3.40405.230623 (4.4.5)
```

In that example we'd use the 4.4 release branch:

```
# Skip the clone if you already installed esp-idf above.
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout --recurse-submodules --force release/v4.4
./install.sh esp32
. ./export.sh
cd
git clone https://github.com/espressif/esp32-arduino-lib-builder
cd esp32-arduino-lib-builder
git checkout --recurse-submodules --force release/v4.4
./build.sh -t esp32 -b idf_libs -A idf-release/v4.4 -I release/v4.4
# Then the sdkconfig will be in one of these locations:
ls out/tools/sdk/esp32/sdkconfig out/tools/esp32-arduino-libs/esp32/sdkconfig
```
