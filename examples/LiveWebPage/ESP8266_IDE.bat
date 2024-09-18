@echo off

rem  This batch file sets up the Arduino IDE with all the correct board options (as found in the IDE "Tools" menu)

rem  Board spec for "Wemos D1 mini"
set BOARDSPEC=esp8266:esp8266:d1_mini:xtal=160,ssl=basic,mmu=3232,non32xfer=fast,eesz=4M1M,ip=hb2n

rem  Fill in your COM port here
set COMPORT=COM3

rem  Get the full directory name of the currently running script
set MYPATH=%~dp0

rem  Launch the Arduino IDE with the specified board options
call "%MYPATH%..\..\extras\Scripts\ArduinoIdeEnv.bat"
