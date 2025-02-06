// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code will run on the **nRF52** to configure the device.  This code will likely be part of our nRF52 firmware.
//
// Created: Chip Audette Feb 2025
// MIT License.  Use at your own risk.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <bluefruit.h>
#include "AT_Processor.h"
#include "BLEUart_Adafruit.h"
#include "BLEUart_Tympan.h"
#include "BLE_LedService.h"

#define MESSAGE_LENGTH 256     // default ble buffer size
// #define OUT_STRING_LENGTH 201
// #define NUM_BUF_LENGTH 11

ble_gap_addr_t this_gap_addr;
#define MAC_NBYTES 6



//   vvvvv  VERSION INDICATION  vvvvv
const char versionString[] = "TympanBLE v0.4.0, nRF52840";
char deviceName[] = "TympanF-TACO"; // gets modified with part of the uniqueID
const char manufacturerName[] = "Flywheel Lab";

// BLE
uint16_t handle;
char BLEmessage[MESSAGE_LENGTH];
boolean bleConnected = false;
boolean bleBegun = false;
String uniqueID = "DEADBEEFCAFEDATE"; // [16]; // used to gather the 'serial number' of the chip
char bleInChar;  // incoming BLE char
BLEService *serviceToAdvertise = nullptr;

//Create the nRF52 BLE elements (the firmware on the nRF BLE Hardware itself)
BLEDfu            bledfu;  // Adafruit's built-in OTA DFU service
BLEDis            bledis;  // Adafruit's built-in device information service
BLEUart_Tympan    bleUart_Tympan;    //Tympan extension of the Adafruit UART service that allows us to change the Service and Characteristic UUIDs
BLEUart_Adafruit  bleUart_Adafruit;  //Adafruit's built-in UART service
BLE_LedButtonService_4bytes    ble_lbs_4bytes;
//AT_Processor    AT_interpreter(&bleUart_Tympan, &SERIAL_TO_TYMPAN);  //interpreter for the AT command set that we're inventing
AT_Processor      AT_interpreter(&bleUart_Tympan, &bleUart_Adafruit, &SERIAL_TO_TYMPAN);  //interpreter for the AT command set that we're inventing

// Define a container for holding BLE Services that might need to get invoked independently later
#define MAX_N_PRESET_SERVICES 10
BLE_Service_Preset* all_service_presets[MAX_N_PRESET_SERVICES];
bool flag_activateServicePreset[MAX_N_PRESET_SERVICES] = {true, true, true, false, false, false, false, false, false, false};
int service_preset_to_ble_advertise = 1;  //which of the presets to include in the advertising.  could be overwritten

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

bool enablePresetServiceById(int preset_id, bool enable) {
  if ((preset_id > 0) && (preset_id < MAX_N_PRESET_SERVICES)) {
    return flag_activateServicePreset[preset_id] = enable;
  }
  return false;
}

int setAdvertisingServiceToPresetById(int preset_id) {
  if ((preset_id > 0) && (preset_id < MAX_N_PRESET_SERVICES)) {
      service_preset_to_ble_advertise = preset_id;

      //in case this function gets called after the system is running (or about to begin()), follow through with the next steps, too
      BLE_Service_Preset* service_ptr = all_service_presets[service_preset_to_ble_advertise];
      if (service_ptr) serviceToAdvertise = service_ptr->getServiceToAdvertise();
      return service_preset_to_ble_advertise;
  }
  return -1;
}

void beginAllBleServices(int setup_config_id) {
  int preset_id = 0;

  bleBegun = true;
  // set the MAC address
  Bluefruit.setAddr(&this_gap_addr);

  //set the unique (has 16 HEX characters)!
  if (DEBUG_VIA_USB) { Serial.print("nRF52840 Firmware: setting BLE name: "); Serial.println(deviceName); };
  Bluefruit.setName(deviceName);
  
  //setup the connect and disconnect callbacks
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin(); // makes it possible to do OTA DFU

  // Configure and Start Device Information Service
  bledis.setManufacturer(manufacturerName); //"Flywheel Lab");
  bledis.setModel(versionString);
  bledis.begin();

  // Configure and begin all of the other services (as requested)
  for (preset_id == 1; preset_id < MAX_N_PRESET_SERVICES; preset_id++) {  //start at 1, assuming 0 is always the dfu service
    if (flag_activateServicePreset[preset_id]) {
      switch (preset_id) {
        case 1:
          bleUart_Tympan.begin(preset_id); all_service_presets[preset_id] = &bleUart_Tympan;  //begin the service and add it to the array holding all active services
          break;
        case 2:
          bleUart_Adafruit.begin(preset_id); all_service_presets[preset_id] = &bleUart_Adafruit; //begin the service and add it to the array holding all active services
          break;
        case 3:
          ble_lbs_4bytes.begin(preset_id); all_service_presets[preset_id] = &ble_lbs_4bytes; //begin the service and add it to the array holding all active services
          break;
        //add more cases here  

      }
    }
  }

  //get which service to advertise
  setAdvertisingServiceToPresetById(service_preset_to_ble_advertise);

  //start advertising
  startAdv();
 }

void setupBLE(){
  // Disable pin 19 LED function. We don't use pin 19
  Bluefruit.autoConnLed(false);
  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  // other config***() ??

  //start the basic BLE stuff (not the services) and populate the intitial names
  Bluefruit.begin(); 
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values

  //get the default MAC
  Bluefruit.getAddr(this_gap_addr.addr);
  // Serial.println("nRF62_BLE_Stuff: beginAllBleServices: init_mac: ");
  // for (int I=0; I<MAC_NBYTES; I++) Serial.print(this_gap_addr.addr[MAC_NBYTES-1-I],HEX);
  // Serial.println();

  //set the default name
  deviceName[8] = uniqueID.charAt(11); // [10];
  deviceName[9] = uniqueID.charAt(12); // [11];
  deviceName[10] = uniqueID.charAt(13); // [12];
  deviceName[11] = uniqueID.charAt(14); // [13];

  //this sets up all the BLE services and characteristics
  //beginAllBleServices();
}

uint8_t hexCharToNibble(char c) {
    if (c >= '0' && c <= '9') {
        return (uint8_t)(c - '0');
    } else if (c >= 'A' && c <= 'F') {
        return (uint8_t)(c - 'A' + 10);
    } else if (c >= 'a' && c <= 'f') {
        return (uint8_t)(c - 'a' + 10);
    } else { //invalid
        return 0; 
    }
}

uint8_t hexCharsToByte(char c1, char c2) {
  return (hexCharToNibble(c1) << 4) + hexCharToNibble(c2);
}

void setMacAddress(char* addr_chars) {
  this_gap_addr.addr_type=BLE_GAP_ADDR_TYPE_PUBLIC;

  //convert hexidecimal characters to bytes...and reverse the order of the bytes
  for (int Ibyte = 0; Ibyte < 6; Ibyte++) {
    char c1 = addr_chars[Ibyte*2];
    char c2 = addr_chars[Ibyte*2+1];
    this_gap_addr.addr[MAC_NBYTES-1-Ibyte]=hexCharsToByte(c1,c2);
  }

  //the MAC will actually get set when the services are started
}

void startAdv(void)
{
  if (bleBegun == false)  return;

  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  if (serviceToAdvertise != nullptr) Bluefruit.Advertising.addService(*serviceToAdvertise); //set which BLE service to advertise

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


int sendBleDataByServiceAndChar(int command, int service_id, int char_id, int nbytes, const uint8_t *databytes) {
  if (DEBUG_VIA_USB) { 
      Serial.print("sendBleDataByServiceAndChar: BLE Command " + String(command) + ", service = " + String(service_id));
      Serial.print(", char " + String(char_id) + ", nbytes = " + String(nbytes));
      Serial.print(", data bytes = "); Serial.write(databytes, nbytes);
      Serial.println();
  }
  //find the service that matches
  int Iservice = 0;
  bool data_sent = false;
  while (Iservice < MAX_N_PRESET_SERVICES) {
    BLE_Service_Preset *service_ptr = all_service_presets[Iservice];
    if (!service_ptr) {
      //this is not a valid service pointer
      //Serial.println("sendBleDataByServiceAndChar: comparing given service_id " + String(service_id) + " to UNINITIALIZED preset service");
    } else {
      //this is a valid service pointer
      //Serial.println("sendBleDataByServiceAndChar: comparing given service_id " + String(service_id) + " to preset service " + String(service_ptr->service_id));
      if (service_ptr->service_id == service_id) {
        if (command == 1) {
          if (DEBUG_VIA_USB) {
            Serial.print("sendBleDataByServiceAndChar: BLE WRITE to characteristic " + String(char_id) + ", data = ");
            Serial.write(databytes, nbytes);
            Serial.println();
          }
          service_ptr->write(char_id, databytes,nbytes); data_sent = true;
        } else if (command == 2) {
          if (DEBUG_VIA_USB) {
            Serial.print("sendBleDataByServiceAndChar: BLE NOTIFY to characteristic " + String(char_id) + ", data = ");
            Serial.write(databytes, nbytes); 
            Serial.println(); 
          }
          service_ptr->notify(char_id, databytes,nbytes);data_sent = true;
        }
      }
    }
    Iservice++;  //increment to look at next service in the list
  }
  if ((data_sent == false) && DEBUG_VIA_USB) Serial.println("sendBleDataByServiceAndChar: data NOT sent to service " + String(service_id) + " because no matching service ID found");
  if (data_sent == false) return -1;
  return 0;
}

