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
        pulseaudio --kill
        rm ~/.config/pulse/client.conf

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

7. Reboot

8. Configure the router (if using with TCL):

    Change router password to 'otto'
    Enable WiFi as FISHLIGHT, with password 6503355358
    Change router IP to 192.168.60.1/255.255.255.0
    Set DHCP range as 192.168.60.60-254
    Set MAC-based static IP for computer: 192.168.60.178
    Disable router's connection to the Internet (if any)
    Refresh IP address by unplugging/plugging the cable and/or:
        sudo dhclient -r ; sudo dhclient
        ifconfig
    (for WGR614: http://www.routerlogin.com/basicsetting.htm)


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

3. Preprocess clips (the results will go into `env/clips`):

        make clips

4. Run the player (without TCL):

        ./start.py --disable-net

5. If there is no audio, run 'aplay -l' and update
   MPD_CARD_ID in dfplayer/player.py.

6. Some useful commands:

        git config --global core.editor "vim"
        git push origin mybranch:master
        git submodule update --init
        apcaccess status


After the player is running you can control it via browser UI. Navigate to 
http://*yourhost*:8080 and enjoy :)

Happy hacking!
