DiscoFish Player
================

Media player software for [DiscoFish art car](http://www.discofish.org/).


Getting started
---------------

The player should happily run on most UNIXy systems. Steps below assume
you're on some Debian flavour, e.g. Ubuntu. Modify accordingly for other 
platforms.

1. Install `ffmpeg` and `mpd` if you don't have them yet. Make sure they are in
$PATH.  


        sudo apt-get install -y ffmpeg
        sudo apt-get install -y mpd

2. Make sure you have python headers and `virtualenv`:
		
		sudo apt-get install -y python-dev
        sudo apt-get install -y python-setuptools
        sudo easy_install pip
        sudo pip install virtualenv

3. Clone this repo:

        git clone git@bitbucket.org:azov/dfplayer.git $PROJECT_DIR
        cd $PROJECT_DIR

4. Download some videos and put them into the `$PROJECT_DIR/clips` directory. 
For now the script will only look for `*.mp4` files.


5. Run `make` to create virtual environment in `$PROJECT_DIR/env`, 
build and install dependencies, and do the rest of setuptools magic. **If it fails 
the first time try re-running it again** (some upstream packages have their 
dependency checking screwed up??) The whole process will take a while.

        make develop

6. Preprocess clips (the results will go into `$PROJECT_DIR/env/clips`):

        make clips

7. Run the player:

        env/bin/dfplayer --listen=0.0.0.0:8080

You can also type `make run` instead of steps 5-7 if you're feeling lazy.

After the player is running you can control it via browser UI. Navigate to 
http://*yourhost*:8080 and enjoy :)

Happy hacking!
