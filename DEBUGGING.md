# To symbolicate a stack trace

First install the [ESP IDF tools](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html)

The in a terminal run this once: `. $HOME/src/esp-idf/export.sh`
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

## Vests

```
pio run -e ds33_dev_a -t upload && pio device monitor -e ds33_dev_a
pio run -e ds33_dev_b -t upload && pio device monitor -e ds33_dev_b
pio run -e ds33_dev_c -t upload && pio device monitor -e ds33_dev_c
```

## Creatures

```
pio run -e ds33_creature_r -t upload && pio device monitor -e ds33_creature_r
pio run -e ds33_creature_g -t upload && pio device monitor -e ds33_creature_g
pio run -e ds33_creature_b -t upload && pio device monitor -e ds33_creature_b
```

## Orrery

```
pio run -e orrery_a -t upload && pio device monitor -e orrery_a
pio run -e orrery_b -t upload && pio device monitor -e orrery_b
pio run -e orrery_c -t upload && pio device monitor -e orrery_c
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

Additionally, this mode leverages the `sdkconfig.vest_instrumentation` file in this repository. To keep this file up
to date with changes to the `esp-idf` and `arduino-esp32 projects`, we need to generate it ourselves. That can be
done with the commands below.

First, determine which version of esp-idf is used by default. There should be a line like the following in the output
of `pio run -e vest_instrumentation`:

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

Then we copy it to this project and make the required edits:

```
cp esp32-arduino-lib-builder/out/tools/esp32-arduino-libs/esp32/sdkconfig jazzlights/sdkconfig.vest_instrumentation
cd jazzlights
# Close Visual Studio
rm -rf .pio build
# Open Visual Studio (required to reinitialize .pio directory)
pio run -e vest_instrumentation -t menuconfig
```

We then need the following changes made to enable instrumentation:

```
CONFIG_FREERTOS_USE_TRACE_FACILITY=y
CONFIG_FREERTOS_USE_STATS_FORMATTING_FUNCTIONS=y
CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER=y
```

In an ideal world, we'd prefer to replace `sdkconfig` with `sdkconfig.defaults` as documented
[here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/kconfig.html#using-sdkconfig-defaults).
However, that doesn't currently work with PlatformIO environments, see
[this issue](https://github.com/platformio/platform-espressif32/issues/638).
