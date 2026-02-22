@echo off

rem  This batch file sets up the Arduino IDE with all the correct board options (as found in the IDE "Tools" menu)

rem   Board spec for "Lilygo TTGO T7 V1.3 Mini32"
set BOARDSPEC=esp32:esp32:esp32:CPUFreq=240,FlashFreq=80,FlashSize=4M,PartitionScheme=default,DebugLevel=none
rem   Alternatively:
rem set BOARDSPEC=esp32:esp32:ttgo-t7-v13-mini32:CPUFreq=240,FlashFreq=80,FlashSize=4M,PartitionScheme=default,DebugLevel=none

rem  Fill in your COM port here
set COMPORT=COM3

rem  Get the full directory name of the currently running script
set MYPATH=%~dp0

rem  Launch the Arduino IDE with the specified board options
call "%MYPATH%..\..\extras\Scripts\ArduinoIdeEnv.bat"
