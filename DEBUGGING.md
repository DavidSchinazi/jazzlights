# To symbolicate a stack trace

First install the [ESP IDF tools](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/linux-macos-setup.html)

The in a terminal run this once: `. $HOME/esp/esp-idf/export.sh`
Then `xtensa-esp32-elf-addr2line` will be in the PATH and can be used as follows:

`xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/atom_matrix_dev/firmware.elf 0x400d34cc:0x3ffced20 0x400d33da:0x3ffced60 0x401c9779:0x3ffcee00 0x400d4276:0x3ffcee20 0x400d3c4b:0x3ffcee80 0x400d41d1:0x3ffceed0 0x400d424e:0x3ffcef90 0x400d5011:0x3ffcefb0 0x400d11fb:0x3ffcf000 0x400db62c:0x3ffcf020 0x40090a8e:0x3ffcf040`

Otherwise the location of `xtensa-esp32-elf-addr2line` can be found by running `find $HOME/.espressif/tools/xtensa-esp32-elf/ -name xtensa-esp32-elf-addr2line`

# To run from the command line

## PlatformIO build for ESP32

Run from `unisparks`:
```
pio run -e atom_matrix_dev -t upload && pio device monitor -e atom_matrix_dev
```

## Rust build for tglight

Run from `unisparks/tglight`:
```
make && ./build/debug/tglight --timestamp --config ./etc/tglight-test.toml
```

# DS33 dual devices

```
pio run -e ds33_dev_a -t upload && pio device monitor -e ds33_dev_a
pio run -e ds33_dev_b -t upload && pio device monitor -e ds33_dev_b
```
