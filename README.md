DiscoFish Player
================

Media player software for [DiscoFish art car](http://www.discofish.org/).


Computer Setup
--------------

The player runs Ubuntu 14.04. Steps below assume you're on some
Debian flavour, e.g. Ubuntu. Modify accordingly for other 
platforms.

1. Install git:

        sudo apt-get install git

2. Clone this repo:

        git clone https://bitbucket.org/discofish/dfplayer.git dfplayer
        cd dfplayer

2.5 Clone dfsparks repo:

        git clone https://bitbucket.org/discofish/dfsparks.git dfsparks
        cd dfsparks 

2.5.1 Follow instructions in dfsparks README.txt to build and install dfsparks

3. Install other required packages:

        sudo ./instal-deps.py

4. Disable PulseAudio:

        sudo vi /etc/pulse/client.conf
            autospawn = no
            daemon-binary = /bin/true
        pulseaudio --kill ; rm ~/.config/pulse/client.conf

5. Set up sound loopback:

        sudo vi /etc/modules
          Add snd-aloop
        sudo vi /etc/modprobe.d/sound.conf
          Add lines:
            options snd_usb_audio index=10
            options snd_aloop index=11

6. Set up UPS daemon (if used):

        sudo vi /etc/apcupsd/apcupsd.conf
          Change UPSTYPE and UPSCABLE to usb
          Comment out DEVICE
          Set TIMEOUT to 60
        sudo vi /etc/default/apcupsd
          Set ISCONFIGURED to yes
        sudo ln -s /etc/init.d/apcupsd /etc/rc2.d/S20apcupsd

7. Configure Ubuntu

        While installing:
          Use fish / .... as username and password
          Enable auto-login
        Install updates
        Add terminal to quick-launch
        All Settings / Brightness & Lock
          Do not turn off the screen
          Do not lock
          Do not require password
        # All Settings / Appearance / Behavior
        #   Auto-hide the Launcher
        Click on networking, disable WiFi
        Click on networking, Edit Connections / Wired Connection 2
          Verify it is "eth1", go to IPv4 Settings
            Set Method to Manual
            Add address: 192.168.60.178 / 255.255.255.0
            Save
        Enable dfplayer to run at startup
          cp dfplayer.desktop ~/.config/autostart/

8. Reboot


Physical Setup
--------------

1. Use a separate USB for DAC

2. Connect UPS, keyboard and mouse through a USB hub

3. Connect "eth1" (central networking port) to router

4. Configure the router for TCL communication:

    1. Use NetGear N300 WNR3500L (or better something that supports DD-WRT)
    2. Advanced/Setup/Internet Setup:
       - Connect yellow (NetGear's Internet) port to the Internet
       - Apply without making changes
    3. Advanced/Setup/LAN Setup:
       - Set IP address to 192.168.60.1/255.255.255.0
       - Set DHCP range as 192.168.60.60-254
       - Apply
    4. Advanced/Setup/Wireless Setup:
       - Set WiFi as FISHLIGHT, with password our password
       - Apply
    5. Advanced/Administration/Set Password:
       - Change admin password to our password
    (for WGR614 use http://www.routerlogin.com/basicsetting.htm)


Running the Player
------------------

1. Download some videos and audio files, and put them into `clips`
directory. The script will only look for `mp4` and `m4a` files.

2. Run `make develop` to create virtual environment in `env`,
build and install dependencies, and do the rest of setuptools magic.
**If it fails the first time try re-running it again** (some upstream
packages have their dependency checking screwed up??) The whole process
will take a while.

        make develop

3. Compile C++ code:

        make cpp

4. Preprocess clips (the results will go into `env/clips`):

        make clips

5. Run the player (without TCL):

        ./start.py --disable-net --mpd

6. If there is no audio, run 'aplay -l' and update
   MPD_CARD_ID in dfplayer/player.py.

7. Some useful commands:

        git config --global core.editor "vim"
        git push origin mybranch:master
        git submodule update --init
        apcaccess status
        Refresh IP address by unplugging/plugging the cable and/or:
          sudo dhclient -r ; sudo dhclient
          ifconfig


After the player is running you can control it via browser UI. Navigate to 
http://*yourhost*:8080 and enjoy :)


Installing OpenKinect (work in progress)
----------------------------------------

  This should actually not be needed. TODO(igorc): Remove.

  Note that the first command will likely expect sudo password.

  wget -O- http://neuro.debian.net/lists/$(lsb_release -cs).us-nh | sudo tee /etc/apt/sources.list.d/neurodebian.sources.list
  sudo apt-key adv --recv-keys --keyserver pgp.mit.edu 2649A5A9
  sudo apt-get update
  sudo apt-get install freenect

  Copy https://github.com/OpenKinect/libfreenect/blob/master/platform/linux/udev/51-kinect.rules
    into /etc/udev/rules.d/51-kinect.rules
  sudo udevadm control --reload-rules

  Or compile:
    sudo apt-get install git-core cmake freeglut3-dev pkg-config build-essential libxmu-dev libxi-dev libusb-1.0-0-dev
    git clone https://github.com/OpenKinect/libfreenect.git
    mkdir build ; cd build ; cmake -L ..
    make

  You may want to modify examples/glview.c to remove MOTOR subdevice.

  Issues:
    https://github.com/OpenKinect/libfreenect/issues/402


  Update PIP:
    sudo pip install --upgrade pip

  Installing Raspbian on BananaPi:
    Download and unzip M1 build from http://www.bananapi.com/index.php/download
    Insert SD card, look for device name in `sudo fdisk -l` (e.g. 8GB size)
    sudo dd if=2015-01-31-raspbian-bpi-r1.img of=/dev/sdX bs=1M
    Boot BananaPi, open `sudo raspi-config`:
      Extend partion size
      Change locale to en-US
      Reboot
    

Happy hacking!
