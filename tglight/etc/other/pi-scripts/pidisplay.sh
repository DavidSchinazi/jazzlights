#!/usr/bin/env bash
### Raspberry Pi 3B+ configurator V0.1
# This script runs the appropriate setup for a Raspberry Pi 3B+ to act as a BMS display
# In addition to other tasks the primary objective of this script is to do the following
# Be as minimal as possible, boot straight to BMS OI, be stable and fast
# be secure, enable easy access to the BMS for an Engineer.
#
# V0.1 of this script is fairly hacky and doesn't really check for errors very well.
# It also will not stop on errors, so it's easy to have a configuration fail without knowing.
# Future versions should add functions for these processes and integrate more error logging.
#
# Primary sources for these configurations can be found at the following URLs
# https://die-antwort.eu/techblog/2017-12-setup-raspberry-pi-for-kiosk-mode/
# https://www.raspberrypi.org/documentation/configuration/wireless/access-point.md
# https://github.com/emoncms/emoncms/blob/master/docs/RaspberryPi/Low-write-mode.md
# https://github.com/emoncms/emoncms/blob/master/docs/RaspberryPi/read-only.md
# https://scribles.net/customizing-boot-up-screen-on-raspberry-pi/
#
# To run this script, flash raspbian-lite to an sd card and install in the pi
# Ensure a network cable is connected and boot the pi.
# On first login run "sudo service ssh start" and then "ifconfig" to find the IP
# copy this script and related files with "scp myfiles pi@PI_IP_ADDRESS:/tmp" (use -r for dirs)
# On the Pi, (not ssh) login with root and run this script

set -x
#set -e

##########
# Begin setting variables
##########
# Set operator. This is the user with which the 
OPERATOR="pi"
# Operator password. Ensure this is encoded with crypt via "openssl passwd -crypt PASSWORD"
# The Default password is built from "NuvationBMS!"
#OPERATOR_PASS="playachino" 

#BEGIN AUTOMATED PROCESS
#PROMPT FOR USER INPUT

# Ensure script is run as sudo
if (( $EUID != 0 )); then
    echo "Please run as sudo or root"
    exit
fi

#Prompt user for raspi-config process
echo "This script REQUIRES raspi-config to have been run already."
echo
echo -n "Has raspi-config been fully configured? [y/N] "
read
if [[ ! "$REPLY" =~ ^(yes|y|Y)$ ]]; then
	echo "Canceled."
	exit 0
fi

# Install directory for Nuvation BMS Operator Interface
OI_DIR="/opt/kiosk"
oidir_input=${OI_DIR}
read -p "Enter the directory for the kiosk name: "  oidir_input
echo "Setting kiosk directory to ${oidir_input}"
OI_DIR=${oidir_input}

# Device hostname
HOSTNAME="pikiosk"
hostname_input=$HOSTNAME
read -p "Enter the hotname for this device: "  hostname_input
echo "Setting hostname to ${hostname_input}"
HOSTNAME=${hostname_input}

# WiFi variables
WIFI_SSID=""

WIFI_PASS=""
# Current user
CUSER=$(who -m  | awk '{ print $1; }')
# Get server IP address
MACHINEIP=$(ip route get 1.1.1.1 | awk '{print $NF; exit}')

# Ensure the script is run as root
#if [[ $CUSER != "root" ]]; then
#    echo "Error: script must be run as root"
#    echo "You must LOGOUT OF THE CURRENT USER and login as root"
#    exit 1
#fi

# FIXME make this check for operator user work correctly
# Check if operator user exists
#if [[ id "${OPERATOR}" ]]; then
#    echo "Error: Operator user "${OPERATOR}" exists."
#    exit 1
#fi

# Explicitly abort if the OI directory exists
if [[ -d "$OI_DIR" ]]; then
    echo "Error: OI directory "${OI_DIR}" already exists." >&2
    exit 1
fi

# Gather User input
# Set wlan0 AP name
echo ifconfig wlan0
read -p "Enter the AP name: "  ssid_input
echo "Setting WiFi AP to $ssid_input"
WIFI_SSID=$ssid_input
# Set AP password
read -p "Enter the WiFi password: "  password_input
echo "Setting WiFi password to $password_input"
WIFI_PASS=$password_input

# Build BMS user
export LANGUAGE=en_US.UTF-8
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8
locale-gen en_US.UTF-8

# adduser --disabled-password --gecos "" ${OPERATOR}
# echo ${OPERATOR}:${OPERATOR_PASS} | chpasswd
# usermod -a -G sudo ${OPERATOR}
# mkdir /home/${OPERATOR}
# chown ${OPERATOR}:${OPERATOR} /home/${OPERATOR}

# run configurator
#sudo -u ${OPERATOR} sudo raspi-config

# Create BMS software dir
mkdir ${OI_DIR} 
chown www-data:www-data ${OI_DIR}
chmod 755 ${OI_DIR}

# Update apt & default packages
apt update
apt upgrade

# Install haveged for better entropy for WPA PSKs
apt-get install -y haveged 

# Install X11 openbox and chromium
apt-get install -y --no-install-recommends xserver-xorg x11-xserver-utils xinit openbox chromium-browser lighttpd

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
chromium-browser --user-data-dir=/tmp/chromium --disable-infobars --kiosk 'file://${OI_DIR}/live/index.html'

EOF

# Bash autostart options
cat >> /home/${OPERATOR}/.bash_profile <<EOF
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
# Enable WiFi AP mode
apt-get install -y dnsmasq hostapd
systemctl stop dnsmasq
systemctl stop hostapd

mv /etc/dnsmasq.conf /etc/dnsmasq.conf.orig  

cat >> /etc/dnsmasq.conf <<EOT
interface=wlan0      # Use the require wireless interface - usually wlan0
dhcp-range=192.168.4.2,192.168.4.20,255.255.255.0,24h
resolv-file=/tmp/resolv.conf
dhcp-leasefile=/tmp/dnsmasq.leases
EOT

cat >> /etc/dhcpcd.conf <<EOF

# static IP address only when no DHCP
interface eth0
fallback nodhcp

profile nodhcp
static ip_address=192.168.1.11/24
static routers=192.168.1.1/24
static domain_name_servers=192.168.1.1

# Static IP for WiFi AP
interface wlan0
static ip_address=192.168.4.1/24
EOF
service dhcpcd restart

cat > /etc/hostapd/hostapd.conf <<EOT

interface=wlan0
ssid=${WIFI_SSID}
driver=nl80211
hw_mode=g
channel=1
macaddr_acl=0
auth_algs=1
ignore_broadcast_ssid=0
wpa=3
wpa_passphrase=${WIFI_PASS}
wpa_key_mgmt=WPA-PSK
wpa_pairwise=TKIP
rsn_pairwise=CCMP
EOT

sed -i \
's/#DAEMON_CONF=""/DAEMON_CONF="\/etc\/hostapd\/hostapd.conf"/' \
"/etc/default/hostapd"

systemctl start hostapd
systemctl start dnsmasq

# Wait for services to start again
sleep 5

sed -i \
'/net.ipv4.ip_forward=1/s/^#//g' \
"/etc/sysctl.conf"

iptables -t nat -A  POSTROUTING -o eth0 -j MASQUERADE
sh -c "iptables-save > /etc/iptables.ipv4.nat"

sed -i \
'/exit 0/iiptables-restore < /etc/iptables.ipv4.nat' \
/etc/rc.local

# Disable login for "pi" user. We are not deleting this user due to various dependencies on
# this user account within raspbian by default.
#sed -i \
#'s#pi:$6$4zuXqr4n$KwuHglaqrV7BAlAggtTmH5HVcH8ClzDpY3X1Z0xXgwRKNl1L2Y3EBNJ9wqa8Y6pblam36bK7hzeRXi80Iy27Z0:#pi:NP:#' \
#"/etc/shadow"

#Symlinks for Readonly
mkdir -p /home/pi/.local/share/
chown pi:pi /home/pi/.local /home/pi/.local/share
ln -s /tmp /home/pi/.local/share/xorg

# Setup lighttpd
cat > /etc/lighttpd/lighttpd.conf <<EOT
server.modules = (
	"mod_access",
	"mod_alias",
	"mod_compress",
 	"mod_redirect",
)

server.document-root        = "${OI_DIR}/live/"
server.upload-dirs          = ( "/var/cache/lighttpd/uploads" )
server.errorlog             = "/tmp/lighttpderror.log"
server.pid-file             = "/var/run/lighttpd.pid"
server.username             = "www-data"
server.groupname            = "www-data"
server.port                 = 80


index-file.names            = ( "index.php", "index.html", "index.lighttpd.html", "Nuvation_BMS_Customer_Starter_Kit.html" )
url.access-deny             = ( "~", ".inc" )
static-file.exclude-extensions = ( ".php", ".pl", ".fcgi" )

compress.cache-dir          = "/var/cache/lighttpd/compress/"
compress.filetype           = ( "application/javascript", "text/css", "text/html", "text/plain" )

# default listening port for IPv6 falls back to the IPv4 port
include_shell "/usr/share/lighttpd/use-ipv6.pl " + server.port
include_shell "/usr/share/lighttpd/create-mime.assign.pl"
include_shell "/usr/share/lighttpd/include-conf-enabled.pl"

EOT

# Run readonly magic script
echo "Configuration complete."
echo "Please run \"sudo bash read-only-fs.sh\" as the ${OPERATOR} user"
echo "Default selections for a BMS display are:"
echo "Jumper: Yes"
echo "GPIO-halt: No"
echo "watchdog: Yes"
