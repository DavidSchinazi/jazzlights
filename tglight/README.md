# TechnoGecko LED Control Software

## Setting up Raspberry Pi boxes

### Hardware

We're using Raspberry Pi Model 3 B+ as our target platform. For boxes that need displays,
[the official 7" touch screen](https://www.raspberrypi.org/products/raspberry-pi-touch-display/) is
a good choice, and it fits nicely in [Smart Pi Touch Case](https://www.amazon.com/dp/B01HKWAJ6K/?coliid=I36LJJCYT5XK94&colid=1GJJNYQVILRYR&psc=0&ref_=lv_ov_lig_dp_it).

It would be nice to add realtime clock to this configuration, but we haven't tried it yet.

### OS configuration

Unfortunately, I don't have all details on how LIGHT01 box was configured, but here are some pointers.

Start with the latest [Raspbian Stretch Desktop](https://www.raspberrypi.org/downloads/raspbian/). I'm using 2018-06-27 version
on my dev box. Create an SD card.

On Linux, you can do it like this:

```shell
	lsblk
	# Find the right dev number, e.g. /dev/sdb (or /dev/mmcblk0) with partitions /dev/sdb1 (/dev/mmcblk0p1)
	# Unmount all partitions:
	umount /dev/sdb1
	umount /dev/sdb2
	# ...
	unzip -p 2018-04-18-raspbian-stretch.zip | sudo dd of=/dev/sdX1 bs=4M conv=fsync

	# Enable ssh
	touch /media/boot/ssh
```

You can (and probably should) skip the next step for boxes that don't need WiFi connection (i.e. most of our boxes), but
in case you want to set it up, edit `/media/boot/wpa_supplicant.conf` to look like this:

```
# /boot/wpa_supplicant.conf -> /etc/wpa_supplicant/wpa_supplicant.conf
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
country=us
network={
	ssid="TechnoGecko"
	psk="theshiniestlizard"
	key_mgmt=WPA-PSK
}
```
(LIGHT01 and WTF01 boxes are currently configured to create WiFi network instead. I'm not sure it's a good idea. We don't really need it and
we'll have a router - and more than enough WiFi noise already - on the playa).

Eject the SD card, insert it into Raspberry Pi, connect ethernet cable, and boot. You should see it under `raspberrypi` name (if not, login into
router and check DHCP lease table to find it's IP), we'll refer to it as `{TGLIGHT_HOST}`.

Then connect to TechnoGecko WiFi (the password is `theshiniestlizard`) and type:

```shell
	# Add your SSH key to the Pi box. The default password for 'pi' user is 'paspberry'
	cat ${HOME}/.ssh/id_rsa.pub | ssh pi@${TGLIGHT_HOST} 'mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys'
	ssh pi@${TGLIGHT_HOST} "sudo mkdir -p /root/.ssh && sudo cp ~/.ssh/authorized_keys /root/.ssh"

	# Change pi user password to 'otto' so that we don't get security warnings about default password
	ssh root@${TGLIGHT_HOST} "passwd pi"

	# Add tglight deploy keys to the box
	cat etc/tglight_deploy_id_rsa | ssh root@${TGLIGHT_HOST} 'cat > ~/.ssh/id_rsa'
	cat etc/tglight_deploy_id_rsa.pub | ssh root@${TGLIGHT_HOST} 'cat > ~/.ssh/id_rsa.pub'

	# Edit /etc/dhcpcd.conf and set IP address to 10.1.0.1, 10.1.0.2, etc.
	# Edit /etc/hosts and /etc/hostname and rename the box to light01, light02, etc.
	# Edit /etc/dhcpcd.conf and set static IP to 10.1.0.31 for light01, 10.1.0.32 for light02, etc.
```

Then we need to set it to run Chromium in kiosk mode, opening the page http://localhost:80 on boot.
I did it on my dev box by editing `/home/pi/.config/lxsession/LXDE-pi/autostart`, LIGHT01 is set up
differently (the file is `/etc/xdg/openbox/autostart`, but I'm not sure if you need to set up anything else
for it to work).

Turn off mouse pointer (I do it by  `sudo apt-get install -y unclutter`, not sure how it's set on LIGHT01).

Set it up to mount flash card on boot (via /etc/fstab), and to mount root filesystem as read-only (again, I'm not
sure how exactly it is set up on LIGHT01, probably via /etc/fstab as well).

Then follow the deployment instructions below.

## Making and testing code changes

Most of pattern generaton code now lives in
[Unisparks repository on Github](https://github.com/DavidSchinazi/unisparks). This repository
is for the stuff that depends on TechnoGecko hardware.

The easies way to get started is to use Docker. Install Docker (`sudo apt-get install docker.io` on Ubuntu<sup>1</sup>, or download and run installers for [Mac](https://www.docker.com/docker-mac) or [Windows](https://www.docker.com/docker-windows)), then type:

```shell
    # check out and build
	git clone https://github.com/DavidSchinazi/tglight.git
	cd tglight
	make docker-build

	# run
	make docker-run
```

Then open web browser and navigate to http://localhost:8080 - you should see LED control UI.

<sup>1</sup> Ubuntu 18.04 LTE or later. If you're using an older version look up the appropriate commands.

## Deploying software updates

Connect to TechnoGecko WiFi (the password is `theshiniestlizard`) and type:

```shell
	# update LED control software on light01 box
	TGLIGHT_HOST=light01 make docker-deploy
```

## Developing without Docker

To run without Docker (e.g. if you recompile a lot and want better performance, or use native debugger, or something of
this sort) - look at the `Dockerfile` and setup your environment in a similar way. You may have to modify commands to
work on your system.


## Cloning SD card on Mac

To create master image from card:

```shell
	diskutil list
	# Identify SD card

	sudo dd if=/dev/diskN of=dist/tglight.img
```

To flash image onto card:

```shell
	diskutil list
	# Identify SD card

	diskutil unmountDisk /dev/diskN
	sudo newfs_msdos -F 16 /dev/diskN
	sudo dd if=dist/tglight.img of=/dev/diskN
```

## Debugging

To watch tglight logs, `ssh` to the box (or attach a keyboard) and run

```shell
	journalctl -utglight -f
```


## Strand Map

### Robot

The entire robot is currently powered from one strand

### Caboose

* Strand 0 - Caboose left (inside) tail fins
* Strand 1 - Caboose right (outside) tail fins
* Strand 2 - Caboose outside wall
* Strand 3 - Caboose inside wall
* Strand 4 - Caboose dorsal fins D1 + D2 + D3
* Strand 5 - Caboose dorsal fins D4 + D5 + D6
* Strand 6 - Caboose dorsal fins D7 + D8 + D9
* Strand 7 - Caboose dorsal fins D10 + D11 + D12
