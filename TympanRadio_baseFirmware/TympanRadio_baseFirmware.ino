/*

      basefirmwareJ-link.ino
      
      This is the most basic code base for flashing Tympan Ref F BLE module
      * Bluetooth connection handlers
      * Generates unique advertising name specific to the module
      * Includes OTA DFU Service 
        >>  ALL TYMPAN nRF52  CODE MUST INCLUDE OTA DFU SERVICE  <<
      * Basic comms over BLE to blink LEDs for testing coms pipeline
      
      THIS FIRMWARE IS FOR FLASHING OVER J-LINK
      TARGETTING FOB BLANK MDBT50Q-1MV2 nRF52840 MODULE

 
    Made by Joel Murphy for Flywheel Lab, February 2024
 */

#include <Arduino.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "TympanBLE.h"

BLEDfu  bledfu;  // OTA DFU service
BLEDis  bledis;  // device information
BLEUart bleuart; // uart over ble


void setup() {
  Serial.begin(115200);
  unsigned long t = millis();
  int timeOut = 1000; // 1 second time out before we bail on a serial connection
  while (!Serial) { // use this to allow for serial to time out
    if(millis() - t > timeOut){
      usingSerial = false;
      Serial.end();
      break;
    }
  }
  delay(1000);
  setupBLE();
  // start advertising
  startAdv();
  if(usingSerial){
    Serial.println("advertising as "); Serial.println(versionString);
    Serial.println("connect and send '?' for options");
  }
  for(int i=0; i<3; i++){
    pinMode(ledPin[i],OUTPUT);
  }
  ledToFade = blue; // initialize this as you like
  // fadeDelay = FADE_DELAY_FAST;
  fadeDelay = FADE_DELAY_SLOW;
  lastShowTime = millis();
}



void loop() {

    if(!printedHelp){ printHelp(); }
    if(!printedBLEhelp){ printBLEhelp(); }
    
    if(ledToFade > 0){ showRGB_LED(millis()); }

  if(usingSerial){ serialEvent(); }
  if(bleConnected){ BLEevent(); }

}


void showRGB_LED(unsigned long m){
  if(m > lastShowTime + fadeDelay){
    lastShowTime = m; // keep time
    fadeValue += fadeRate;  // adjust brightness
    if(fadeValue >= FADE_MIN){ // LEDs are Common Anode
      fadeValue = FADE_MIN; // keep in bounds
      fadeRate = 0 - FADE_RATE; // change direction
    } else if(fadeValue <= FADE_MAX){ // LEDs are Common Anode
      fadeValue = FADE_MAX; // keep in bounds
      fadeRate = 0 + FADE_RATE; // change direction
    }
  analogWrite(ledToFade,fadeValue);
  }
}

void LEDsOff(){
  for(int i=0; i<3; i++){
    analogWrite(ledPin[i],FADE_MIN);
  }
}