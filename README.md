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

        git clone https://bitbucket.org/azov/dfplayer.git dfplayer
        cd dfplayer

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
        sudo vi /etc/default/apcupsd
          Set ISCONFIGURED to yes
        sudo ln -s /etc/init.d/apcupsd /etc/rc2.d/S20apcupsd

7. Configure Ubuntu

        While installing:
          Use fish / otto as username and password
          Enable auto-login
        Install updates
        Add terminal to quick-launch
        All Settings / Brightness & Lock
          Do not turn off the screen
          Do not lock
          Do not require password
        Click on networking, disable WiFi
        Click on networking, Edit Connections / Wired Connection 2
          Verify it is "eth1", go to IPv4 Settings
            Set Method to Manual
            Add address: 192.168.60.178 / 255.255.255.0
            Save

8. Reboot


Physical Setup
--------------

1. Use a separate USB for DAC

2. Connect UPS, keyboard and mouse through a USB hub

3. Connect "eth1" (central networking port) to router

4. Configure the router for TCL communication:

    1. Use NetGear N300 WNR3500L (or better something that supports DD-WRT)
    2. Advanced/Setup/Internet Setup:
       - Apply without making changes
    3. Advanced/Setup/LAN Setup:
       - Set IP address to 192.168.60.1/255.255.255.0
       - Set DHCP range as 192.168.60.60-254
       - Apply
    4. Advanced/Setup/Wireless Setup:
       - Set WiFi as FISHLIGHT, with password 155155155
       - Apply
    5. Advanced/Administration/Set Password:
       - Change admin password to 'otto'
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

        ./start.py --disable-net

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

Happy hacking!
