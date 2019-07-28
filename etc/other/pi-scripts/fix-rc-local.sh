#!/bin/bash

cat > /etc/rc.local <<EOT
#!/bin/sh -e
#
# rc.local
#
# This script is executed at the end of each multiuser runlevel.
# Make sure that the script will "exit 0" on success or any other
# value on error.
#
# In order to enable or disable this script just change the execution
# bits.
#
# By default this script does nothing.

# Print the IP address
_IP=$(hostname -I) || true
if [ "$_IP" ]; then
  printf "My IP address is %s\n" "$_IP"
fi
iptables-restore < /etc/iptables.ipv4.nat
service hostapd stop
sleep 5
service hostapd start
gpio -g mode 21 up
if [ `gpio -g read 21` -eq 0 ] ; then
  ln -s /home/pi/.config/chromium/ /tmp/chromium
  mount -o remount,rw /
  mount -o remount,rw /boot
else
  cp -r /home/pi/.config/chromium /tmp/chromium && chown -R pi:pi /tmp/chromium
fi
exit 0

EOT
