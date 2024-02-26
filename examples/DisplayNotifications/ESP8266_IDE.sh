#!/usr/bin/bash

# This script sets up the Arduino IDE with all the correct board options (as found in the IDE "Tools" menu)

# Board spec for "Wemos D1 mini"
BOARDSPEC=esp8266:esp8266:d1_mini:xtal=160,ssl=basic,eesz=4M1M,ip=lm2n

# Fill in your COM port here
COMPORT=/dev/ttyUSB0

# Get the full directory name of the currently running script
\cd `dirname $0`
MYPATH=`pwd`
\cd - > /dev/null

# Launch the Arduino IDE with the specified board options
. "${MYPATH}/../../extras/Scripts/ArduinoIdeEnv.sh"
