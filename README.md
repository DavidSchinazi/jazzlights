# Unisparks Library

[![Build Status](https://travis-ci.org/unisparks/unisparks.svg?branch=master)](https://travis-ci.org/unisparks/unisparks)

> **Note: The API and network protocol are not yet stable and expected to change before v.1**


Unisparks is a library for building networked LED art. We use it to create
wearables, costumes, accessories, even robots and art cars that display
synchronized light patterns. Check it out if you're tinkering with NeoPixels
and similar electronics. And next time you're at Burning Man - go find
DiscoFish and blink with us! :)

Features: 

- Works on embedded and desktop platforms
- Compartible with Arduino IDE
- ESP8266, Ubuntu Linux, and OSX supported out of the box
- Easy to modify for other Arduino boards or UNIXy systems

## Using the library

### Using with Arduino IDE

To install Arduino and unisparks code on Ubuntu:

```shell
    # Download latest software from https://www.arduino.cc/en/Main/Softwarea
    # https://www.arduino.cc/download_handler.php?f=/arduino-1.6.10-linux64.tar.xz
    cd ~
    mv Downloads/arduino-1.6.10-linux64.tar.xz ./
    tar -xf arduino-1.6.10-linux64.tar.xz
    mkdir -p ~/Arduino/libraries
    cd ~/Arduino/libraries
    git clone https://igor_c@bitbucket.org/discofish/unisparks.git Unisparks
    git clone https://github.com/FastLED/FastLED
    ~/arduino-1.6.10/arduino &
    # Open File/Preferences, and add to Additional Boards:
        http://arduino.esp8266.com/stable/package_esp8266com_index.json
    Open Tools/Boards/BoardsManager, search for esp8266, install
    Select Tools/Boards/NodeMCU1.0
    Add yourself to dialout group:
        sudo usermod -a -G dialout igor
```

To compile:

```shell
    ~/arduino-1.6.10/arduino &
    Click menu option: File/Examples/Unisparks/Vest
    Click on the checkmark button to verify
    Select Tools/Ports/ttyUSB0
    Click the Upload button
```

You can use extras/test-server.py to test how your device
synchronizes with the server.

### Building PC library (Ubuntu)

```shell
    sudo apt-get install -y make
    sudo apt-get install -y cmake
	git clone git@bitbucket.org:discofish/unisparks.git
	mkdir unisparks/build
	cd unisparks/build && cmake .. && make 
	sudo make install
```

The last step installs the library system-wide, so that dfplayer build can
find it.


### Building demo app (Ubuntu)

Build PC library as explained above, then install GLFW and build the demo:

```shell
pushd /tmp && git clone https://github.com/glfw/glfw --depth 1 &&\
	cd glfw && cmake . && make && sudo make install && popd 
cd unisparks/build && make unisparks_demo && ./dfspaks_demo
```
 
## Flashing updated firmware

(this is generic stuff not specific to Unisparks library)

To flash new firmware onto NodeMCU:

```shell
# Open https://github.com/nodemcu/nodemcu-firmware/releases
#    download latest firmware
# Otherwise, you can build your own newer firmware:
#    go to http://nodemcu-build.com/
#    enter email address, leave all default, start build
#    download using link from email, e.g.:
#        wget http://nodemcu-build.com/builds/nodemcu-master-7-modules-2016-08-14-10-43-06-float.bin
# Connect the device using USB cable
#    try different cable if first one does not work
cd ~
git clone https://github.com/themadinventor/esptool
sudo python ./esptool/esptool.py --port /dev/ttyUSB0  write_flash 0x00000 \
    nodemcu-master-7-modules-2016-08-14-10-43-06-float.bin
```

## Uploading sketches on OSX

(this is generic stuff not specific to Unisparks library)

To upload sketches to ESP8266 board from Arduino IDE on OSX:

- Install ESP8266 board. Go to Arduino->Preferences->Settings; Additional board manager URLs
- Add http://arduino.esp8266.com/stable/package_esp8266com_index.json
- Install ESP8266 board
- Install USB to UART drivers. Required drivers seem to be different for different 
   board flavours. 
   - For LoLin boards this worked: 
		http://www.codenuke.net/2015/01/nodemcu-install-ch340-usb-to-serial-for-yosemite.html
   - For noname board I got from Amazon this worked: 
		https://www.silabs.com/products/mcu/Pages/USBtoUARTBridgeVCPDrivers.aspx (/dev/cu.SLAB_USBtoUART)
- Use NodeMCU 1.0 (ESP-12E) board and port corresponding to USB driver
- Set serial monitor to 115200 baud
