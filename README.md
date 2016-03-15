DiscoFish Wearables
====================

Installation (Ubuntu)
---------------------

Change directory to project root and run the following commands: 

    sudo apt-get install -y make
	git clone https://github.com/glfw/glfw --depth 1 && cd glfw && cmake . && make && sudo make install && cd ..
    make 
 

Installation (OSX)
---------------------

Change directory to project root and run the following commands: 

	brew tap homebrew/versions
	brew install glfw3
	make