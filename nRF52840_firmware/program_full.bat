::
:: This windows BAT file updates the firmware of the NRF52840 used by the Tympan Rev F.
:: You must use the programming nest; it does not work through the Tympan's USB port.
::
:: See README at https://github.com/Tympan/Tympan_Rev_F_Hardware#programming-with-hardware-connection
::
:: As it says, you need the Nordic "Command Line Tools" from 
:: https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools/download
::
:: Note that the command below is very specific to your local installation.  For example,
:: it was created when the Adafruit nRF52 library was at version 1.6.1, which is referenced
:: within the command line text below.  If you have a different version on your computer
:: this BAT won't work.  You'll need to update it to match the version on your computer.
::
:: Also, the command below assumes that your arduino sketch holding your nRF52840 firmware is
:: titled "nR52840_firmware.ino".  The HEX file resulting from the compilation step is named
:: "nRF52840_fimrware.ino.hex", so if you rename your *.ino, the *.hex will change, too.  You'll
:: have to change the command below to use the new name4ertghjnmk,./
::1
:: When trying to program the device, it must be in the programming nest and the Tympan must
:: be turned on.  So, it must have its battery or plugged into USB.

@echo off
:: Set Path to nRF command line tool
path "C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin\"

:: Store path to adafruit nRF Bootloader v1.6.1
set bootloaderHex=%LOCALAPPDATA%\Arduino15\packages\adafruit\hardware\nrf52\1.6.1\bootloader\feather_nrf52840_express\feather_nrf52840_express_bootloader-0.9.0_s140_6.1.1.hex

:: Store path to Adafruit nRF52840 firmware 
set fwPath=build\adafruit.nrf52.feather52840\nRF52840_firmware.ino.hex

nrfjprog.exe --program %bootloaderHex% --family NRF52 --chiperase --reset --verify && ^
nrfjprog.exe --memwr 0xFF000 --val 0x01 && ^
nrfjprog.exe --reset --program %fwPath% --sectorerase --family NRF52 --verify