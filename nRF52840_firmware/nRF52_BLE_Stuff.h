// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code will run on the **nRF52** to configure the device.  This code will likely be part of our nRF52 firmware.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <bluefruit.h>
#include "nRF52_AT_API.h"

#define MESSAGE_LENGTH 256     // default ble buffer size
// #define OUT_STRING_LENGTH 201
// #define NUM_BUF_LENGTH 11

// ID Strings to be used by the nRF52 firmware to enable recognition by Tympan Remote App
//String serviceUUID = String("BC-2F-4C-C6-AA-EF-43-51-90-34-D6-62-68-E3-28-F0");
//String characteristicUUID = String("06-D1-E5-E7-79-AD-4A-71-8F-AA-37-37-89-F7-D9-3C");
uint8_t serviceUUID[] = { 0xF0, 0x28, 0xE3, 0x68, 0x62, 0xD6, 0x34, 0x90, 0x51, 0x43, 0xEF, 0xAA, 0xC6, 0x4C, 0x2F, 0xBC };
uint8_t characteristicUUID[] = {  0x3C, 0xD9, 0xF7, 0x89, 0x37, 0x37, 0xAA, 0x8F, 0x71, 0x4A , 0xAD, 0x79, 0xE7, 0xE5, 0xD1, 0x06}; 
BLECharacteristic myBleChar = BLECharacteristic(characteristicUUID, BLENotify | BLEWrite); //from #include <bluefruit.h>

//   vvvvv  VERSION INDICATION  vvvvv
const char versionString[] = "TympanBLE v0.3.0, nRF52840";
char deviceName[] = "Tympan-TACO"; // gets modified with part of the uniqueID
const char manufacturerName[] = "Flywheel Lab";

// BLE
uint16_t handle;
//char tempCharArray[MESSAGE_LENGTH];
//int tempCounter;
char BLEmessage[MESSAGE_LENGTH];
boolean bleConnected = false;
//boolean printedBLEhelp = false;
String uniqueID = "DEADBEEFCAFEDATE"; // [16]; // used to gather the 'serial number' of the chip
//char bleInBuffer[MESSAGE_LENGTH];  // incoming BLE buffer
//char bleOutBuffer[MESSAGE_LENGTH]; // outgoing BLE buffer
char bleInChar;  // incoming BLE char
//int bleBufCounter = 0;
//char outString[OUT_STRING_LENGTH];			
//char numberBuffer[NUM_BUF_LENGTH];

//Create the nRF52 BLE elements (the firmware on the nRF BLE Hardware itself)
BLEDfu          bledfu;  // Adafruit's built-in OTA DFU service
BLEDis          bledis;  // Adafruit's built-in device information service
BLEUart_Tympan  bleService_tympanUART;    //Tympan extension of the Adafruit UART service that allows us to change the Service and Characteristic UUIDs
BLEUart         bleService_adafruitUART;  //Adafruit's built-in UART service
//nRF52_AT_API    AT_interpreter(&bleService_tympanUART, &SERIAL_TO_TYMPAN);  //interpreter for the AT command set that we're inventing
nRF52_AT_API    AT_interpreter(&bleService_tympanUART, &bleService_adafruitUART, &SERIAL_TO_TYMPAN);  //interpreter for the AT command set that we're inventing

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  handle = conn_handle; // save this for when/if we disconnect
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  bleConnected = true;
  Serial.print("nRF52840 Firmware: connect_callback: Connected to "); Serial.print(central_name);
  Serial.print(", bleConnected = "); Serial.println(bleConnected);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;
  bleConnected = false;
  Serial.print("nRF52840 Firmware: disconnect_callback: Disconnected, reason = 0x"); Serial.print(reason, HEX);
  Serial.print(", bleConnected = "); Serial.println(bleConnected);
}


/*
 *  BLEwrite()
 *    Main function that sends data over bluetooth
 *    Checks for open connection
 *    Can send up to 64 ascii characters
 */
// int BLEwrite(){
//   int success = 0;
//   // airString += String('\n');
//   int counter = 0;
//   for(int i=0; i<OUT_STRING_LENGTH; i++){
// 		if(outString[i] == 0){ break; }
//     BLEmessage[i] = outString[i];
// 		counter++;
//   }
// 	BLEmessage[counter] = '\n';
// 	counter++;
//   if(bleConnected){
//     success = bleuart.write( BLEmessage, counter );
//   }else{
//     success = -1;
//   }
//   return success;
// }

/*
 *  BLEwriteInt(int)
 *  concatinates the int with outString
 *  Then calles BLEwrite
 */
// void BLEwriteInt(int i){
//   sprintf(numberBuffer, "%d",i);
//   strcat(outString,numberBuffer);
//   BLEwrite();
// }

/*
 *  BLEwriteFloat(String,float,int)
 *  Combines a string with a float and number of decimal places
 *  Then calles BLEwrite
 */
// void BLEwriteFloat(float f,int dPlaces){
//   sprintf(numberBuffer, "%.*f", dPlaces, f);
//   strcat(outString,numberBuffer);
//   BLEwrite();
// }

int BLEevent(BLEUart *bleuart_ptr, HardwareSerial *serial_to_tympan) {
  int success = -1;
  if(bleuart_ptr->available()) {
    Serial.print("nRF52840 Firmware: BLEevent: available bytes = "); Serial.println(bleuart_ptr->available());
    success = 0;
    Serial.print("    : message = ");
    while (bleuart_ptr->available()) {
      char c = bleuart_ptr->read();
      Serial.print(c);
      serial_to_tympan->write(c);
    }
    Serial.println();
  }
  return success;
}

void serialEvent(HardwareSerial *serial_from_tympan) { //for the nRF firmware, service any messages coming in the serial port from the Tympan
    //interpret received characters as part of AT command set
    if(serial_from_tympan->available()) {
      //Serial.print("nRF52840 Firmware: serialEvent: available bytes = "); Serial.println(serial_from_tympan->available());
      while (serial_from_tympan->available()) AT_interpreter.processSerialCharacter(serial_from_tympan->read());
    }
 }

void setupBLE(){
  // Disable pin 19 LED function. We don't use pin 19
  Bluefruit.autoConnLed(false);
  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  // other config***() ??
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  uniqueID = getMcuUniqueID();
  // unique ID has 16 HEX characters!
  deviceName[7] = uniqueID.charAt(11); // [10];
  deviceName[8] = uniqueID.charAt(12); // [11];
  deviceName[9] = uniqueID.charAt(13); // [12];
  deviceName[10] = uniqueID.charAt(14); // [13];
  Bluefruit.setName(deviceName);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin(); // makes it possible to do OTA DFU
  // Configure and Start Device Information Service
  bledis.setManufacturer(manufacturerName); //"Flywheel Lab");
  bledis.setModel(versionString);
  bledis.begin();

  // Configure the standard Adafruit UART service
  bleService_adafruitUART.begin();

  // Configure and Start our custom tympan-specific BLE Uart Service
  bleService_tympanUART.setUuid(serviceUUID);
  bleService_tympanUART.setCharacteristicUuid(myBleChar);
  bleService_tympanUART.begin();

  // Start our custom service ...this is all now done in bleService_tympanUART.begin();
  //myBleChar.setProperties(CHR_PROPS_NOTIFY);  //is this needed?
  //myBleChar.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS); //is this needed?
  //myBleChar.begin();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  // Include bleuart 128-bit uuid
  //Bluefruit.Advertising.addService(bleService_adafruitUART); //is this necessary?
  Bluefruit.Advertising.addService(bleService_tympanUART);   //this one is necessary for the TympanRemote App to see this device

  //Bluefruit.Advertising.addService(myBleService);
  // Add 'Name' to Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   *
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

void stopAdv(void) {
  Bluefruit.Advertising.stop();
}


