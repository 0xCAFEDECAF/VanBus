# Launch the Arduino IDE
#
# Variables used:
# * BOARDSPEC - colon-separated list of board (hardware) specifications
#   E.g.: BOARDSPEC=esp8266:esp8266:esp8285:ssl=basic,ResetMethod=ck,eesz=1M64,led=13,ip=lm2n
# * COMPORT (optional) - COM port
#   E.g.: COMPORT=/dev/ttyUSB0

MYPATH=${MYPATH%/}  # Strip trailing slash (if any)
MYFOLDER=${MYPATH##*/}
MAIN_INO=${MYFOLDER}.ino

[[ -n $COMPORT ]] && COMPORT_PARAM="--port ${COMPORT}"

# Pre-set the board settings
arduino $COMPORT_PARAM --board $BOARDSPEC --save-prefs

# Now launch the IDE
arduino "${MYPATH}/${MAIN_INO}"
