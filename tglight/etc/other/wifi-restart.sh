#!/bin/bash 
iwconfig wlan0 power off 
while true ; do if ping -c2 fritz.box > /dev/null; then sleep 120 else echo "Network connection down! Attempting reconnection." ifdown wlan0 ifdown --force wlan0 killall dhclient ifup wlan0 iwconfig wlan0 power off sleep 60 fi done
