/*
      nRF52840_Firmware
      
      This is the Tympan-written firmware for the nRF52840 BLE device that is built into the Tympan
      Rev F.  The device provides BLE communication between the Tympan and mobile devices, especially
      when the mobile device is running the TympanRemote App.

      See Documentation here: https://github.com/Tympan/Tympan_Rev_F_Hardware/wiki/Bluetooth-and-Tympan-Rev-F
            
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

    MIT License, use at your own risk.
 */

#define DEBUG_VIA_USB true

#define SERIAL_TO_TYMPAN Serial1                 //use this when physically wired to a Tympan. Assumes that the nRF is connected via Serial1 pins
#define SERIAL_FROM_TYMPAN Serial1               //use this when physically wired to a Tympan. Assumes that the nRF is connected via Serial1 pins

// Include the files needed by nRF52 firmware
#include <Arduino.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
#include "BLE_Generic.h"
#include "BLEUart_Adafruit.h"
#include "BLE_BleDis.h"
#include "BLEUart_Tympan.h"
#include "BLE_Stuff.h"
#include "LED_controller.h"
#include "AT_Processor.h"  //must already have included LED_control.h
#include "USB_SerialManager.h"


// ///////////////////////////////// Helper Functions

#define GPIO_for_isConnected 25  //what nRF pin is connected to "MISO1" net name
void setupGPIO(void) {
  pinMode(GPIO_for_isConnected, OUTPUT);
  digitalWrite(GPIO_for_isConnected, LOW);
}

LED_controller led_control;


void issueATCommand(const String &str) {  issueATCommand(str.c_str(), str.length()); }
void issueATCommand(const char *msg, unsigned int len_msg) {
  if (DEBUG_VIA_USB) {
    Serial.print(F("nRF52840 Firmware: sending to be interpreted as AT command: "));
    for (unsigned int i=0; i<len_msg; i++) Serial.print(msg[i]);
    Serial.println();
  }      
  for (unsigned int i=0; i<len_msg; i++) AT_interpreter.processSerialCharacter(msg[i]);
  AT_interpreter.processSerialCharacter('\r');  //add carriage return
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
    Serial.println(F("*** nRF52840 Firmware: STARTING ***"));
  }

  //start the nRF's UART serial port that is physically connected to a Tympan or other microcrontroller (if used)
  Serial1.setPins(0,1);   //our nRF wiring uses Pin0 for RX and Pin1 for TX
  Serial1.begin(115200);  
  //Serial1.begin(115200, SERIAL_8N1, 0,1,9,10);
  delay(500);
  while (Serial1.available()) Serial1.read();  //clear UART buffer

  //setup the GPIO pins
  setupGPIO();

  //initialize the LED display
  led_control.setLedColor(led_control.red);
  
  //setup BLE and begin
  setupBLE();    //as of Feb 2025, does not automatically start the BLE services
  //startAdv();  // start advertising

  if (DEBUG_VIA_USB) printHelpToUSB();
}


void loop(void) {

  //Respond to incoming messages on the USB serial
  if (DEBUG_VIA_USB) {
    if (Serial.available()) serialManager_processCharacter(Serial.read());
  }
  

  //Respond to incoming UART serial messages
  serialEvent(&SERIAL_FROM_TYMPAN);  //for the nRF firmware, service any messages coming in the serial port from the Tympan
  
  //Respond to incoming BLE messages
  if (bleBegun && bleConnected) { 
    //for the nRF firmware, service any messages coming in from BLE wireless link
    if (bleUart_Tympan.has_begun) BLEevent(&bleUart_Tympan, &SERIAL_TO_TYMPAN);  
    if (bleUart_Adafruit.has_begun) BLEevent(&bleUart_Adafruit, &SERIAL_TO_TYMPAN);  
  }

  //Service out-going BLE comms that have been queued by the AT-style processor
  AT_interpreter.update(millis());

  //periodically update the delay used for queued comms in case the BLE connection interval has changed
  serviceBleQueueDelayViaBleConnInterval(millis());

  //service the LEDs
  serviceLEDs(millis());

  //service the GPIO pins
  serviceGPIO(millis());
}

// ///////////////////////////////// Servicing Functions

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

void serviceBleQueueDelayViaBleConnInterval(unsigned long curTime_millis) {
  static unsigned long lastUpdate_millis = 0;

  if (curTime_millis < lastUpdate_millis) lastUpdate_millis = 0;  //handle time wrap-around
  if ((curTime_millis - lastUpdate_millis) > 250UL) {  // how often to update
    int conn_interval_msec = getConnectionInterval_msec();
    if (conn_interval_msec > 0) AT_interpreter.updateBleConnectionInterval_msec(conn_interval_msec);
    lastUpdate_millis = curTime_millis;
  }

} 


// ////////////////////////////////////////////////////////////////////// 
/*
* DataStreams: Besides the single-character and four-character modes, there are also
			data streaming modes to support specialized communication.  These special modes
			are not inteded to be invoked by a user's GUI, so they can be ignored.  To avoid
			inadvertently invoking these modes, never send characters such as 0x02, 0x03, 0x04.
			In fact, you should generally avoid any non-printable character or you risk seeing
			unexpected behavior.

			Datastreams expect the following message protocol.  Note that the message length and
			payload are sent as little endian (LSB, MSB):
			 	1.	DATASTREAM_START_CHAR 	(0x02)
				2.	Message Length (int32): number of bytes including parts-4 thru part-6
				3.	DATASTREAM_SEPARATOR 	(0x03)
				4.	Message Type (char): Avoid using the special characters 0x03 or 0x04.  (if set
							to ‘test’, it will print out the payload as an int, then a float)
				5.	DATASTREAM_SEPARATOR 	(0x03)
				6.	Payload
				7.	DATASTREAM_END_CHAR 	(0x04)

			Use RealTerm to send a 'test' message: 
			0x02 0x0D 0x00 0x00 0x00 0x03 0x74 0x65 0x73 0x74 0x03 0xD2 0x02 0x96 0x49 0xD2 0x02 0x96 0x49 0x04
				1. DATASTREAM_START_CHAR 	(0x02)
				2.	Message Length (int32): (0x000D) = 13 
				3.	DATASTREAM_SEPARATOR 	(0x03)
				4.	Message Type (char): 	(0x74657374) = 'test'
				5.	DATASTREAM_SEPARATOR 	(0x03)
				6.	Payload					(0x499602D2, 0x499602D2) = [1234567890, 1234567890]
				7.	DATASTREAM_END_CHAR 	(0x04)
*/
#define DATASTREAM_START_CHAR (0x02)
#define DATASTREAM_SEPARATOR 	(0x03)
#define DATASTREAM_END_CHAR 	(0x04)
void globalWriteBleDataToTympan(const int service_id, const int char_id, uint8_t data[], const size_t len) {
  if (len <= 0) return;

  //prepare for transmission
  char msg_type[] = "BLEDATA";
  char service_id_txt[3] = {0};
  if (service_id < 10) { service_id_txt[0] = service_id + '0'; } else { uint32_t tens = (int)(service_id/10); service_id_txt[0] = tens + '0'; service_id_txt[1] = (service_id - 10*tens) + '0'; }
  char char_id_txt[3] = {0};
  if (char_id < 10) { char_id_txt[0] = char_id + '0'; } else { uint32_t tens = (int)(char_id/10); char_id_txt[0] = tens + '0'; char_id_txt[1] = (char_id - 10*tens) + '0'; }
  uint32_t tot_len = strlen(msg_type) + 1 + strlen(service_id_txt) + 1 + strlen(char_id_txt) + 1 + len;
  
  // Copy to a byte array
  uint32_t header_len = 1+4+1;
  uint32_t footer_len = 1;
  uint32_t msg_len = header_len + tot_len + footer_len;
  uint8_t msg[msg_len];
  uint32_t next_char = 0;
  msg[next_char++] = DATASTREAM_START_CHAR;
  for (int i=0; i<4; i++) msg[next_char++] = (uint8_t)(0x000000FF & (tot_len >> (i*8)));
  msg[next_char++] = DATASTREAM_SEPARATOR;
  for (int i=0; i<strlen(msg_type); i++) msg[next_char++] = msg_type[i];
  msg[next_char++] = (uint8_t)' '; //space character
  for (int i=0; i<strlen(service_id_txt); i++) msg[next_char++] = service_id_txt[i];
  msg[next_char++] = (uint8_t)' '; //space character
  for (int i=0; i<strlen(char_id_txt); i++) msg[next_char++] = char_id_txt[i];
  msg[next_char++] = (uint8_t)' '; //space character;
  for (int i=0; i<len; i++) msg[next_char++] = data[i];
  msg[next_char++] = DATASTREAM_END_CHAR;
  if ((next_char-1) > msg_len) {
    if (DEBUG_VIA_USB) {
      Serial.print(F("globalWriteBleDataToTympan: *** ERROR ***: message length ("));
      Serial.print(next_char);
      Serial.print(F(") is larger than allocated ("));
      Serial.print(msg_len);
      Serial.println(")");
    }
  }

  //send the data
  if (DEBUG_VIA_USB) { Serial.print(F("globalWriteBleDataToTympan: Sending: ")); Serial.write(msg,msg_len);Serial.println(); }
  SERIAL_TO_TYMPAN.write(msg,msg_len);
}

