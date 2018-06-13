#!/bin/bash
ARDUINO_DIST=https://downloads.arduino.cc/arduino-1.8.5-linux64.tar.xz
UNISPARKS_HOME="${UNISPARKS_HOME:-$(dirname $(dirname $(dirname "$0")))}"

set -e
apt-get update
apt-get install -y zip curl xz-utils build-essential default-jre

echo "Installing Arduino"
mkdir -p /opt && curl -s ${ARDUINO_DIST} | tar xvJC /opt
ln -s /opt/arduino-1.8.5/arduino /usr/bin/arduino
ln -s /opt/arduino-1.8.5/arduino-builder /usr/bin/arduino-builder

echo "Installing Arduino libraries"
arduino --install-library "FastLED"
arduino --install-library "Adafruit NeoPixel"

echo "Installing boards"
arduino --pref "boardsmanager.additional.urls=http://arduino.esp8266.com/stable/package_esp8266com_index.json" --save-prefs
arduino --install-boards esp8266:esp8266 

echo "Installing our library"
ln -s ${UNISPARKS_HOME} /root/Arduino/libraries/Unisparks
