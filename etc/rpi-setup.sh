#!/usr/bin/env bash
set -x

# Ensure script is run as sudo
if (( $EUID != 0 )); then
    echo "Please run as sudo or root"
    exit
fi


# If we've started from lite image, install X11 openbox and chromium
apt update
apt-get install -y --no-install-recommends xserver-xorg x11-xserver-utils xinit openbox chromium-browser

# Openbox autostart options
cat > /etc/xdg/openbox/autostart <<EOF
# Disable any form of screen saver / screen blanking / power management
xset s off
xset s noblank
xset -dpms

# Allow quitting the X server with CTRL-ATL-Backspace
setxkbmap -option terminate:ctrl_alt_bksp

# Start Chromium in kiosk mode
sed -i 's/"exited_cleanly":false/"exited_cleanly":true/' /tmp/chromium/'Local State'
sed -i 's/"exited_cleanly":false/"exited_cleanly":true/; s/"exit_type":"[^"]\+"/"exit_type":"Normal"/' /tmp/chromium/Default/Preferences
chromium-browser --user-data-dir=/tmp/chromium --disable-infobars --kiosk 'http://localhost:80'

EOF

# Bash autostart options
cat >> /home/pi/.bash_profile <<EOF
export XAUTHORITY="/tmp/.Xauthority"
[[ -z \$DISPLAY && \$XDG_VTNR -eq 1 ]] && startx -- -nocursor
EOF

# Set display options
cat >> /boot/config.txt <<EOT
# Disable color test
disable_splash=1
start_x=0

# Rotate display 180 degrees
lcd_rotate=2

# Disable warning icon (lightning bolt)
avoid_warnings=1
EOT

cat >> /boot/cmdline.txt <<EOT
logo.nologo vt.global_cursor_default=0
EOT

cat >> /etc/dhcpcd.conf <<EOF

# static IP address only when no DHCP
interface eth0
fallback nodhcp

profile nodhcp
static ip_address=10.1.0.100/8
static routers=10.1.0.1/8
static domain_name_servers=10.1.0.1

EOF
service dhcpcd restart

#Symlinks for Readonly
mkdir -p /home/pi/.local/share/
chown pi:pi /home/pi/.local /home/pi/.local/share
ln -s /tmp /home/pi/.local/share/xorg


#
# Setup readonly filesystem
# 
# Given a filename, a regex pattern to match and a replacement string:
# Replace string if found, else no change.
# (# $1 = filename, $2 = pattern to match, $3 = replacement)
replace() {
	grep $2 $1 >/dev/null
	if [ $? -eq 0 ]; then
		# Pattern found; replace in file
		sed -i "s/$2/$3/g" $1 >/dev/null
	fi
}

# Given a filename, a regex pattern to match and a replacement string:
# If found, perform replacement, else append file w/replacement on new line.
replaceAppend() {
	grep $2 $1 >/dev/null
	if [ $? -eq 0 ]; then
		# Pattern found; replace in file
		sed -i "s/$2/$3/g" $1 >/dev/null
	else
		# Not found; append on new line (silently)
		echo $3 | sudo tee -a $1 >/dev/null
	fi
}

# Given a filename, a regex pattern to match and a string:
# If found, no change, else append file with string on new line.
append1() {
	grep $2 $1 >/dev/null
	if [ $? -ne 0 ]; then
		# Not found; append on new line (silently)
		echo $3 | sudo tee -a $1 >/dev/null
	fi
}

# Given a filename, a regex pattern to match and a string:
# If found, no change, else append space + string to last line --
# this is used for the single-line /boot/cmdline.txt file.
append2() {
	grep $2 $1 >/dev/null
	if [ $? -ne 0 ]; then
		# Not found; insert in file before EOF
		sed -i "s/\'/ $3/g" $1 >/dev/null
	fi
}


# Remove unwanted packages
apt-get remove -y --force-yes --purge triggerhappy cron logrotate \
 dphys-swapfile fake-hwclock
apt-get -y --force-yes autoremove --purge

# Replace log management with busybox (use logread if needed)
echo "Installing busybox-syslogd..."
apt-get -y --force-yes install busybox-syslogd; dpkg --purge rsyslog

# Add fastboot, noswap and/or ro to end of /boot/cmdline.txt
append2 /boot/cmdline.txt fastboot fastboot
append2 /boot/cmdline.txt noswap noswap
append2 /boot/cmdline.txt ro^o^t ro

# Move /var/spool to /tmp
rm -rf /var/spool
ln -s /tmp /var/spool

# Move /var/lib/lightdm and /var/cache/lightdm to /tmp
rm -rf /var/lib/lightdm
rm -rf /var/cache/lightdm
ln -s /tmp /var/lib/lightdm
ln -s /tmp /var/cache/lightdm

# Make SSH work
replaceAppend /etc/ssh/sshd_config "^.*UsePrivilegeSeparation.*$" "UsePrivilegeSeparation no"
# bbro method (not working in Jessie?):
#rmdir /var/run/sshd
#ln -s /tmp /var/run/sshd

# Change spool permissions in var.conf (rondie/Margaret fix)
replace /usr/lib/tmpfiles.d/var.conf "spool\s*0755" "spool 1777"

# Move dhcpd.resolv.conf to tmpfs
touch /tmp/dhcpcd.resolv.conf
rm /etc/resolv.conf
ln -s /tmp/dhcpcd.resolv.conf /etc/resolv.conf

# Make edits to fstab
# make / ro
# tmpfs /var/log tmpfs nodev,nosuid 0 0
# tmpfs /var/tmp tmpfs nodev,nosuid 0 0
# tmpfs /tmp     tmpfs nodev,nosuid 0 0
replace /etc/fstab "vfat\s*defaults\s" "vfat    defaults,ro "
replace /etc/fstab "ext4\s*defaults,noatime\s" "ext4    defaults,noatime,ro "
append1 /etc/fstab "/var/log" "tmpfs /var/log tmpfs nodev,nosuid 0 0"
append1 /etc/fstab "/var/tmp" "tmpfs /var/tmp tmpfs nodev,nosuid 0 0"
append1 /etc/fstab "\s/tmp"   "tmpfs /tmp    tmpfs nodev,nosuid 0 0"

