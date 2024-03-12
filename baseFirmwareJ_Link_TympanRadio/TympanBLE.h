

#ifndef TYMPAN_BLE_H
#define TYMPAN_BLE_H

// PINS
#define LED_0 14  // red
#define LED_1 12  // blue
#define LED_2 15  // green

#define MESSAGE_LENGTH 256     // default ble buffer size
#define OUT_STRING_LENGTH 201
#define NUM_BUF_LENGTH 11

//   vvvvv  VERSION INDICATION  vvvvv
const char versionString[] = "TympanBLE v0.1.0";
char deviceName[] = "Tympan-TACO"; // gets modified with part of the uniqueID
const char manufacturerName[] = "Flywheel Lab";

// BLE
uint16_t handle;
char tempCharArray[MESSAGE_LENGTH];
int tempCounter;
char BLEmessage[MESSAGE_LENGTH];
boolean bleConnected = false;
boolean printedBLEhelp = false;
String uniqueID = "DEADBEEFCAFEDATE"; // [16]; // used to gather the 'serial number' of the chip
char bleInBuffer[MESSAGE_LENGTH];  // incoming BLE buffer
char bleOutBuffer[MESSAGE_LENGTH]; // outgoing BLE buffer
char bleInChar;  // incoming BLE char
int bleBufCounter = 0;
char outString[OUT_STRING_LENGTH];			
char numberBuffer[NUM_BUF_LENGTH];

// LED fade stuff
#define FADE_DELAY_SLOW 20
#define FADE_DELAY_FAST 5
#define FADE_RATE 8
#define FADE_MAX 0
#define FADE_MIN 255
int red = LED_0;
int green = LED_1;
int blue = LED_2;
int ledPin[3] = {LED_0, LED_1, LED_2}; // red, blue, green
unsigned int fadeDelay = FADE_DELAY_FAST;
// unsigned int fadeDelay = FADE_DELAY_SLOW;
int fadeRate = 0 - FADE_RATE;
int fadeValue = FADE_MIN;
int ledToFade = blue;
unsigned long lastShowTime;

//
bool printedHelp = true;
bool usingSerial = true;

#endif