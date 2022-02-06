Start with the latest [Raspbian Stretch Lite](https://www.raspberrypi.org/downloads/raspbian/), this guide is based on 2018-06-27 version. Create an SD card. On Linux, you can do it like this:

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

(or use something like Etcher)

