/*
      nRF52840_Firmware
      
      This is the Tympan-written firmware for the nRF52840 BLE device that is built into the Tympan
      Rev F.  The device provides BLE communication between the Tympan and mobile devices, especially
      when the mobile device is running the TympanRemote App.
      
      To compile the code, set the Arduino IDE "board" for the Adafruit "nRF52840 Express".  Then,
      export the binary (HEX) via the Arduino IDE's "Sketch" menu.  Select "Export Compiled Binary".
      
      Be aware that the Arduino IDE cannot push the firmware to the nRF52840 device.  Instead, 
      you must use an nRF command line tool plus a programming nest.

      To use the nRF command line tool, it needs the HEX file created by the Arduino IDE.  I then
      put the Tympan in the programming nest, connect the nest to the PC via USB, turn on the Tympan,
      and then use the BAT file here in this directory.  If the BAT doesn't work for you, you can
      debug the BAT (update the version number in the EXE name in the BAT?) or you can build your
      own command using the info in the readme: 
      https://github.com/Tympan/Tympan_Rev_F_Hardware#programming-with-hardware-connection

      It's also possible to push a firmware update via the Adafruit/nRF mobile apps.  I have done
      it this way, but I prefer using the programming nest.  

      This code includes:
      * Bluetooth connection handlers
      * Generates unique advertising name specific to the module
      * Includes OTA DFU Service 
        >>  ALL TYMPAN nRF52  CODE MUST INCLUDE OTA DFU SERVICE  <<
      * Basic comms over BLE to blink LEDs for testing coms pipeline
      
 
    Original BLE servicing code by Joel Murphy for Flywheel Lab, February 2024
    Extended by Chip Audette, Benchtop Engineering, for Creare LLC, February 2024
 */

#define DEBUG_VIA_USB true

#define SERIAL_TO_TYMPAN Serial1                 //use this when physically wired to a Tympan. Assumes that the nRF is connected via Serial1 pins
#define SERIAL_FROM_TYMPAN Serial1               //use this when physically wired to a Tympan. Assumes that the nRF is connected via Serial1 pins

// Include the files needed by nRF52 firmware
#include <Arduino.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "BLEuart_Tympan.h"
#include "nRF52_BLE_Stuff.h"
#include "LED_controller.h"
#include "nRF52_AT_API.h"  //must already have included LED_control.h

#define GPIO_for_isConnected 25  //what nRF pin is connected to "MISO1" net name

LED_controller led_control;

void printHelpToUSB(void) {
  Serial.println("nRF52840 Firmware: Help:");
  Serial.print(  "   : Version string: "); Serial.println(versionString);
  Serial.println(" : Status:");
  Serial.print(  "   : bleBegun: "); Serial.println(bleBegun);
  Serial.print(  "   : bleConnected: "); Serial.println(bleConnected);
  Serial.println("   : Send 'h' via USB to get this help");
  if (bleBegun == false) {
    Serial.println(" : Configuration:");
    Serial.println("   : Send 'M' to set MAC address to AABBCCEEDDFF");
    Serial.println("   : Send 'b' or 'B' to begin the BE services (default, mode=1)");
    Serial.println("   : Send 'g' or 'G' to begin the BE services (mode=3)");
  }
  Serial.println(" : Trial Commands:");
  Serial.println("   : Send 'J' via USB to send 'J' to the Tympan");
  Serial.println("   : Send '1' to send AT command 'BLENOTIFY 1 1 1 9");
  Serial.println("   : Send '4' to send AT command 'BLENOTIFY 3 0 4 1234");
  Serial.println("   : Send '5' to send AT command 'BLENOTIFY 3 1 4 5678");
}

void setupGPIO(void) {
  pinMode(GPIO_for_isConnected, OUTPUT);
  digitalWrite(GPIO_for_isConnected, LOW);
}

void issueATCommand(const String &str) {
  if (DEBUG_VIA_USB) Serial.println("nRF52840 Firmware: seding to be interpreted as AT command: " + str);            
  for (int i=0; i<str.length(); i++) AT_interpreter.processSerialCharacter(str[i]);
  if (str[str.length()-1] != '\r') AT_interpreter.processSerialCharacter('\r');  //add carriage return

}

void setup(void) {

  if (DEBUG_VIA_USB) {
    //Start up USB serial for debugging
    Serial.begin(115200);
    unsigned long t = millis();
    int timeOut = 2000; // 5 second time out before we bail on a serial connection
    while (!Serial) { // use this to allow for serial to time out
      if(millis() - t > timeOut)  break;  //break out of waiting
    }
    while (Serial.available()) Serial.read();  //clear input buffer
  
    // send some info to the user on the USB Serial Port
    Serial.println("*** nRF52840 Firmware: STARTING ***");
    printHelpToUSB();
  }

  //start the nRF's UART serial port that is physically connected to a Tympan or other microcrontroller (if used)
  Serial1.setPins(0,1);   //our nRF wiring uses Pin0 for RX and Pin1 for TX
  Serial1.begin(115200);  
  delay(500);
  while (Serial1.available()) Serial1.read();  //clear UART buffer

  //setup the GPIO pins
  setupGPIO();

  //initialize the LED display
  led_control.setLedColor(led_control.red);
  
  //setup BLE and begin
  setupBLE();    //as of Feb 2025, does not automatically start the BLE services
  //startAdv();  // start advertising
}


void loop(void) {

  //Respond to incoming messages on the USB serial
  if (DEBUG_VIA_USB) {
    if (Serial.available()) {
      char c = Serial.read();
      //Serial.print("Received Character via USB: " + String(c));
      switch (c) {
        case 'h': case '?':
          printHelpToUSB();
          break;
        case 'b':
          Serial.println("nRF52840 Firmware: starting all BLE services (default)...");
          beginAllBleServices(1);
          break;
        case 'B':
          issueATCommand(String("SET BEGIN=1"));
          break;
        case 'g':
          Serial.println("nRF52840 Firmware: starting all BLE services (preset=3)...");
          beginAllBleServices(3);
          break;
        case 'G':
          issueATCommand(String("SET BEGIN=3"));
          break;
        case 'M':
          issueATCommand(String("SET MAC=AABBCCDDEEFF"));
          break;
        case 'J':
          Serial.println("nRF52840 Firmware: sending J to Tympan...");
          SERIAL_TO_TYMPAN.println("J");
          break;
        case '1':
          issueATCommand(String("BLENOTIFY 1 1 1 9"));
          break;
        case '4':
          issueATCommand(String("BLENOTIFY 3 0 4 1234"));
          break;
        case '5':
          issueATCommand(String("BLENOTIFY 3 1 4 5678"));
          break;
      }
      //while (Serial->available()) AT_interpreter.processSerialCharacter(Serial.read());  
    }
  }

  //Respond to incoming UART serial messages
  serialEvent(&SERIAL_FROM_TYMPAN);  //for the nRF firmware, service any messages coming in the serial port from the Tympan
  
  //Respond to incoming BLE messages
  if (bleBegun && bleConnected) { 
    //for the nRF firmware, service any messages coming in from BLE wireless link
    BLEevent(&bleService_tympanUART, &SERIAL_TO_TYMPAN);  
    BLEevent(&bleService_adafruitUART, &SERIAL_TO_TYMPAN);  
    
  }

  //service the LEDs
  serviceLEDs(millis());

  //service the GPIO pins
  serviceGPIO(millis());
}

// ///////////////////////////////// Service Routines
void serviceLEDs(unsigned long curTime_millis) {
  static unsigned long lastUpdate_millis = 0;

  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0;  //handle time wrap-around
  if ((curTime_millis - lastUpdate_millis) > 100UL) {  // how often to update
    //if (Bluefruit.connected()) {
    if (bleConnected) {
      if ((led_control.ledToFade > 0) && (led_control.ledToFade != led_control.green)) led_control.setLedColor(led_control.green);
    } else {
      if (bleBegun && Bluefruit.Advertising.isRunning()) {
        if ((led_control.ledToFade > 0) && (led_control.ledToFade != led_control.blue)) led_control.setLedColor(led_control.blue);
      } else {
        if ((led_control.ledToFade > 0) && (led_control.ledToFade != led_control.red))led_control.setLedColor(led_control.red);
      }
    }
    if(led_control.ledToFade > 0) led_control.showRGB_LED(curTime_millis);
    if (led_control.ledToFade==0) led_control.LEDsOff();
    lastUpdate_millis = curTime_millis;
  }
} 

void serviceGPIO(unsigned long curTime_millis) {
  static unsigned long lastUpdate_millis = 0;

  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0;  //handle time wrap-around
  if ((curTime_millis - lastUpdate_millis) > 100UL) {  // how often to update
    //if (Bluefruit.connected()) {
    if (bleConnected) {
      digitalWrite(GPIO_for_isConnected, HIGH);
    } else {
      digitalWrite(GPIO_for_isConnected, LOW);
    }
    lastUpdate_millis = curTime_millis;
  }
} 

