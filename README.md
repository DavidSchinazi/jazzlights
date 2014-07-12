DiscoFish Player
================

Media player software for [DiscoFish art car](http://www.discofish.org/).


Getting started
---------------

The player should happily run on most UNIXy systems. Steps below assume
you're on some Debian flavour, e.g. Ubuntu. Modify accordingly for other 
platforms.

1. Install git:

        sudo apt-get install git

2. Clone this repo:

        git clone git@bitbucket.org:azov/dfplayer.git $PROJECT_DIR
        cd $PROJECT_DIR

3. Install other required packages:

        sudo ./instal-deps.py

4. Download some videos and put them into the `$PROJECT_DIR/clips` directory. 
For now the script will only look for `*.mp4` files.

5. Run `make` to create virtual environment in `$PROJECT_DIR/env`, 
build and install dependencies, and do the rest of setuptools magic. **If it fails 
the first time try re-running it again** (some upstream packages have their 
dependency checking screwed up??) The whole process will take a while.

        make develop

6. Preprocess clips (the results will go into `$PROJECT_DIR/env/clips`):

        make clips

7. Disable PulseAudio:

        sudo vi /etc/pulse/client.conf
            autospawn = no
            daemon-binary = /bin/true
        pulseaudio --kill
        rm ~/.config/pulse/client.conf

8. If there is no audio, run 'aplay -l' and update
   MPD_CARD_ID in dfplayer/player.py.

9. Configure the router:

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

10. Run the player:

        env/bin/dfplayer --listen=0.0.0.0:8080

11. Some useful commands:

        git config --global core.editor "vim"
        git push origin mybranch:master
        git submodule update --init

You can also type `make run` instead of steps 5-7 if you're feeling lazy.

After the player is running you can control it via browser UI. Navigate to 
http://*yourhost*:8080 and enjoy :)

Happy hacking!
