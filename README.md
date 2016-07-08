DiscoFish Wearables
====================

Installation (Ubuntu)
---------------------

Change directory to project root and run the following commands: 

    sudo apt-get install -y make
    sudo apt-get install -y cmake
	pushd /tmp && git clone https://github.com/glfw/glfw --depth 1 && cd glfw && cmake . && make && sudo make install && popd 
    make 
 

Installation (OSX)
---------------------

Change directory to project root and run the following commands: 

	brew tap homebrew/versions
	brew install glfw3
	brew install cmake
	make

To upload ESP8266 sketches using Arduino IDE:
	1. Install ESP8266 board. Go to Arduino->Preferences->Settings; Additional board manager URLs
	2. Add http://arduino.esp8266.com/stable/package_esp8266com_index.json
	3. Install ESP8266 board
	4. Install USB to UART drivers (https://www.silabs.com/products/mcu/Pages/USBtoUARTBridgeVCPDrivers.aspx)
	5. Use NodeMCU 1.0 (ESP-12E) board and /dev/cu.SLAB_USBtoUART port; set serial monitor to 115200 baud