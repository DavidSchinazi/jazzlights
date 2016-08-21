DFSparks Library
================

DFSparks is a library for building wearable electronics that synchronizes 
with DiscoFish art car. The target platform is ESP8266 (though it should be
trivial to modify it for any Arduino-compatible board). There's also a 
PC version that we use on the fish itself.

Using with Arduino IDE
----------------------------

To install Arduino and dfsparks code on Ubuntu:

    Download latest software from https://www.arduino.cc/en/Main/Softwarea
      https://www.arduino.cc/download_handler.php?f=/arduino-1.6.10-linux64.tar.xz
    cd ~
    mv Downloads/arduino-1.6.10-linux64.tar.xz ./
    tar -xf arduino-1.6.10-linux64.tar.xz
    mkdir -p ~/Arduino/libraries
    cd ~/Arduino/libraries
    git clone https://igor_c@bitbucket.org/discofish/dfsparks.git DFSparks
    git clone https://github.com/FastLED/FastLED
    ~/arduino-1.6.10/arduino &
    Open File/Preferences, and add to Additional Boards:
        http://arduino.esp8266.com/stable/package_esp8266com_index.json
    Open Tools/Boards/BoardsManager, search for esp8266, install
    Select Tools/Boards/NodeMCU1.0
    Add yourself to dialout group:
        sudo usermod -a -G dialout igor


To compile:

    ~/arduino-1.6.10/arduino &
    Click menu option: File/Examples/DFSparks/Vest
    Click on the checkmark button to verify
    Select Tools/Ports/ttyUSB0
    Click the Upload button

You can use extras/test-server.py to test how your device
synchronizes with the server.

Building PC library (Ubuntu)
----------------------------

    sudo apt-get install -y make
    sudo apt-get install -y cmake
	git clone git@bitbucket.org:discofish/dfsparks.git
	mkdir dfsparks/build
	cd dfsparks/build && cmake .. && make 
	sudo make install

The last step installs the library system-wide, so that dfplayer build can
find it.

Building demo app (Ubuntu)
----------------------------

Build PC library as explained above, then install GLFW and build the demo:

	pushd /tmp && git clone https://github.com/glfw/glfw --depth 1 &&\
		cd glfw && cmake . && make && sudo make install && popd 
    cd dfsparks/build && make dfsparks_demo && ./dfspaks_demo
 

Adding new effects
----------------------------

To implement new effect:

	1. Create a file that contains effect source, e.g. src/dfsparks/effects/coolstuff.h
	(you may keep it header-only or add a .cpp file as well)
	2. Use one of the existing files as an example. At a minimum, you need to extend 
		the Effect class and implement 'doRender' method.
	3. doRender gets Pixels object representing a viewport mapped to device pixels. 
		You can get viewport width and height from the pixels object. You can also find 
		out viewport coordinates of each pixel by calling 'coords' method of the Pixels
		object. timeElapsed() method on the effect itself lets you query the time.
		All times are in milliseconds, stored as int32_t.
	4. Create the effect in src/dfsparks/playlist.cpp, see Repertoire constructor.
	5. Add new effect sources to CMakeLists.txt


Flashing updated firmware
-------------------------

To flash new firmware onto NodeMCU:

    Open https://github.com/nodemcu/nodemcu-firmware/releases
        download latest firmware
    Otherwise, you can build your own newer firmware:
        go to http://nodemcu-build.com/
        enter email address, leave all default, start build
        download using link from email, e.g.:
            wget http://nodemcu-build.com/builds/nodemcu-master-7-modules-2016-08-14-10-43-06-float.bin
    Connect the device using USB cable
        try different cable if first one does not work
    cd ~
    git clone https://github.com/themadinventor/esptool
    sudo python ./esptool/esptool.py --port /dev/ttyUSB0  write_flash 0x00000 \
        nodemcu-master-7-modules-2016-08-14-10-43-06-float.bin


Uploading sketches on OSX
-------------------------

(this is generic stuff not specific to DFSparks library)

To upload sketches to ESP8266 board from Arduino IDE on OSX:

	1. Install ESP8266 board. Go to Arduino->Preferences->Settings; Additional board manager URLs
	2. Add http://arduino.esp8266.com/stable/package_esp8266com_index.json
	3. Install ESP8266 board
	4. Install USB to UART drivers. Required drivers seem to be different for different 
	   board flavours. 
	   - For LoLin boards this worked: 
			http://www.codenuke.net/2015/01/nodemcu-install-ch340-usb-to-serial-for-yosemite.html
	   - For noname board I got from Amazon this worked: 
			https://www.silabs.com/products/mcu/Pages/USBtoUARTBridgeVCPDrivers.aspx (/dev/cu.SLAB_USBtoUART)
	5. Use NodeMCU 1.0 (ESP-12E) board and port corresponding to USB driver
	6. Set serial monitor to 115200 baud

