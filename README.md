# Tympan Rev F Hardware
Tympan Rev F Hardware Design Files For First Production Run, Februrary, 2024.

## Changes From Rev E
For the most part, the design of Rev F is a clone of Rev E. The primary challenge of producing Rev E is that the BC127 BLE Radio module we used is not longer available (Product End Of Life). Here are the major changes in the hardware

- Changed to nRF52840 BLE Radio Module
	- Using part number MDBT50Q-1MV2 from Raytac
	- Module requires bootloader, we use Adafruit Feather nRF52840 Express bootloader
	- Bootloading blank module proceedure see below
- nRF52 does not support analog audio, but evidence exists that it can use I2S
	- I2S connections between Teensy 4.1 and nRF52 on I2S_2 pins
- UART coms with the BC127 were slower than desired
	- The SPI_1 bus is connected to nRF module for higher speed
	- The UART pins are still connected


## Programming The Radio Module
## Hardware Requirements

The blank nRF52 module is programmable using the j-Link connection on pins SWO, SWCLK, SWDIO, RESET, 3V3, and GND. Those module pins are broken out to a header row on the bottom of the board. See connector J7 in the schematic, and the 6 SMT pads on the back layer of the PCB design.

![nRF52 shematic](assets/nRF52_schem.png)
-
![J-Link Pads](assets/Rev_E_Bottom.png)

### Programming Fixture

We have a specific PCB card for bootloading and programming the nRF52 The folder called `Tympan nRF52 Programmer` contains design files for this PCB. This PCB is not 100% necessary for bootloading and programming the radio. As long as you make the correct connections between the J-Link programmer and the 6 pads on the bottom of the board, you will have success using the How To Program instructions below.

### Programmer

The J-Link by Segger is preferred for flashing the bootloader and any application layer onto the nRF52. You can get one from various suppliers. [Adafruit](https://www.adafruit.com/product/2209), [Mouser](https://www.mouser.com/ProductDetail/Segger-Microcontroller/8.08.00?qs=jA5Ki6243on%2Fr15wFqMuRQ%3D%3D), [Digikey](https://www.digikey.com/en/products/detail/segger-microcontroller-systems/8-08-28/4476087) all carry them.

Adafruit also sells a [low-cost J-Link](https://www.adafruit.com/product/3571) for educational purposes. The EDU version is specifically not for use in commercial applications. If you need to re-program your radio from the bootloader up, your personal use of the EDU version will not break that rule!

## Programming With Hardware Connection

There are a couple of routes through different toolchains that are available to bootload the nRF52 module using the J-link. Our current method uses Command Line Tools from Nordic, the makers of the nRF52 chipset. Download the [Command Line Tools](https://www.nordicsemi.com/Products/Development-tools/nrf-command-line-tools/download), and install them to `Program Files (x86)`

**>>> NOTE: This tutorial currently only supports Windows OS**

The other software that you need is Arduino IDE, which is assumed to be installed and updated to the latest version. Arduino will very tidily create the proper compiled files for flashing. Click on `Sketch > Export Compiled Binary` and the IDE will create a folder called `build` inside your Sketch folder. The build folder contains a subfolder specific to the board. Click on `Sketch > Show Sketch Folder` to open it in an explore window. Inside the subfolder you will find 4 files.

- ELF
- HEX
- MAP
- ZIP

Each file is used for different Device Firmware Update (DFU) processes. For the purpose of initial flashing via J-Link, we will use the `.hex` file. You may note that the full name of the file is something like `TympanRadio_baseFirmware.ino.hex`. The inclusion of the .ino in the name should not cause a problem, but if you want you can change the name and remove the .ino. Just make sure to keep the .hex suffix.

We will be flashing a blank nRF52 module with Adafruit's Feather Express bootloader variant. The file we need is a `.hex` file, and it is located in `C:\Users\{USERNAME}\AppData\Local\Arduino15\packages\adafruit\hardware\nrf52\1.6.0\bootloader\feather_nrf52840_express\feather_nrf52840_express_bootloader-0.8.0_s140.6.1.1.hex`. We recommend making a copy of it, and moving it into the same folder that the IDE just made to keep things tidy.

Open a Command Prompt window, and navigate to `C:\Program Files (x86)\Nordic Semiconductor\nrf-command-line-tools\bin`, and then enter the following commands:

`nrfjprog --program {PATH}\feather_nrf52840_express_bootloader-0.8.0_s140_6.1.1.hex -f nrf52 --chiperase --reset && nrfjprog --memwr 0xFF000 --val 0x01 && nrfjprog --reset --program {PATH}\TympanRadio_baseFirmware.ino.hex --sectorerase -fnrf52`

The first command flashes the bootloader and the Bluetooth soft device. The second command sets a bit in 0xFF000 in order to bypass 'virgin program' hurdle. The third command programs the application software. 

## Over The Air Firmware Updates
Any updates to the application software can be done over Bluetooth using Over The Air Device Firmware Update (OTA DFU). This tutorial will currently cover one method for doing this, using Adafruit's Bluefruit Connect app. You need two files to upload which Arduino makes when you click on `Sketch > Export Compiled Binary`. One is the `.hex` file, The other is a `.dat` file. The `.dat` file is contained inside the compressed `.zip` file that was also produced by the compile binary command. Unzip compressed folder, and find the `.dat` file inside of it. Both of these files need to be accessible on the phone, or tablet, or computorb, that you intend to use to perform the OTA DFU.

**NOTE: Your Sketch MUST include the following code to ensure that OTA DFU is persistent!
After your #includes on the main .ino tab activate the DFU Service
`BLEDfu  bledfu;`
then, during the setup() turn on the OTA DFU Service
`bledfu.begin();` Check out the code in TympanRadio_baseFirmware.ino for how it's done.**

In the Bluefruit Connect App, search for and connect to your Tympan board. Select Updates then scroll all the way down to the USE CUSTOM FIRMWARE button way down there, and press it. You will be prompted to find a "Hex File" and an "Init File". The "Hex" is the `.hex` and the "Init" is the `.dat` that we discussed in the previous paragraph. Then press "Start Update". The process can take some time, but there is a status bar that should animate the progress. 

It is advised to close the Adafruit App after the upload process is complete, and then restart if you intend to use it again right away.

