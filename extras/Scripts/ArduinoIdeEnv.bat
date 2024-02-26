rem  Generic script to launch the Arduino IDE
rem
rem  Variables used:
rem  * BOARDSPEC - colon-separated list of board (hardware) specifications
rem    E.g.: BOARDSPEC=esp8266:esp8266:esp8285:ssl=basic,ResetMethod=ck,eesz=1M64,led=13,ip=lm2n
rem  * COMPORT (optional) - COM port
rem    E.g.: COMPORT=COM3

set MYPATH1=%MYPATH:~0,-1%
for %%f in (%MYPATH1%) do set MYFOLDER=%%~nxf
set MAIN_INO=%MYFOLDER%.ino

if defined COMPORT set COMPORT_PARAM=--port %COMPORT%

rem  Fill in your Arduino IDE installation path here
set IDE_PATH=C:\Program Files (x86)\arduino-1.8.19

rem  Pre-set the board settings
"%IDE_PATH%\arduino_debug.exe" %COMPORT_PARAM% --board %BOARDSPEC% --save-prefs

rem  Now launch the IDE
"%IDE_PATH%\arduino.exe" \\"%MYPATH1%\%MAIN_INO%\\"
