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
#include "BLE_BleDis.h"
#include "BLE_BattService.h"
#include "BLE_LedService.h"

#define MESSAGE_LENGTH 256     // default ble buffer size
// #define OUT_STRING_LENGTH 201
// #define NUM_BUF_LENGTH 11

ble_gap_addr_t this_gap_addr;
#define MAC_NBYTES 6
bool was_MAC_set_by_user = false;


//   vvvvv  VERSION INDICATION  vvvvv
const char versionString[] = "TympanBLE v0.5.0, nRF52840";
char deviceName[] = "TympanF-TACO"; // gets modified with part of the uniqueID
const char manufacturerName[] = "Flywheel Lab";

// BLE
uint16_t handle;
char BLEmessage[MESSAGE_LENGTH] ={0};
boolean bleConnected = false;
boolean bleBegun = false;
String uniqueID = "DEADBEEFCAFEDATE"; // [16]; // used to gather the 'serial number' of the chip
char bleInChar;  // incoming BLE char
BLEService *serviceToAdvertise = nullptr;

//Create the nRF52 BLE elements (the firmware on the nRF BLE Hardware itself)
BLEDfu            bledfu;  // Adafruit's built-in OTA DFU service
BLE_BleDis        ble_bleDis;       // Adafruit's built-in device information service, wrapped
BLEUart_Tympan    bleUart_Tympan;   //Tympan extension of the Adafruit UART service that allows us to change the Service and Characteristic UUIDs
BLEUart_Adafruit  bleUart_Adafruit; //Adafruit's built-in UART service
BLE_BattService   ble_battService;  // battery service
BLE_LedButtonService           ble_lbs; //standard Nordic LED Button Serice (1 byte of data)
BLE_LedButtonService_4bytes    ble_lbs_4bytes; //modified Nordic LED Button Service using 4 bytes of data
BLE_GenericService             ble_generic1, ble_generic2;
//AT_Processor    AT_interpreter(&bleUart_Tympan, &SERIAL_TO_TYMPAN);  //interpreter for the AT command set that we're inventing
AT_Processor      AT_interpreter(&bleUart_Tympan, &bleUart_Adafruit, &SERIAL_TO_TYMPAN);  //interpreter for the AT command set that we're inventing

// Define a container for holding BLE Services that might need to get invoked independently later
#define MAX_N_PRESET_SERVICES 10  //adafruit says that the nRF52 library only allows 10 to be active at one time?
const int max_n_preset_services = MAX_N_PRESET_SERVICES;
BLE_Service_Preset* all_service_presets[MAX_N_PRESET_SERVICES];
BLE_Service_Preset* activated_service_presets[MAX_N_PRESET_SERVICES];
bool flag_activateServicePreset[MAX_N_PRESET_SERVICES];   //set to true to activate that preset service
int service_preset_to_ble_advertise;  //which of the presets to include in the advertising.  will be set in setup

int getConnectionInterval_msec(void) {
  if (!bleConnected) return -1;
  BLEConnection* connection = Bluefruit.Connection(handle);
  if (connection == nullptr) return -1;

  //see https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/4a2d8dd5be9686b6580ed2249cae43972922572f/libraries/Bluefruit52Lib/src/BLEConnection.cpp#L108
  //which eventually traces to: https://github.com/adafruit/Adafruit_nRF52_Arduino/blob/4a2d8dd5be9686b6580ed2249cae43972922572f/cores/nRF5/nordic/softdevice/s132_nrf52_6.1.1_API/include/ble_gap.h#L712
  return (int)(((float)connection->getConnectionInterval()*1.25f) + 0.5f); //in looking at Adafruit docs, it looks like the units are 1.25msec...so I multiply by 1.25 to get the units I want
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  handle = conn_handle; // save this for when/if we disconnect
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  bleConnected = true;
  Serial.print(F("nRF52840 Firmware: connect_callback: Connected to ")); Serial.print(central_name);
  Serial.print(F(", bleConnected = ")); Serial.println(bleConnected);
  //Serial.print(F(", connection_interval (msec) =")); Serial.println(getConnectionInterval_msec());

  AT_interpreter.updateBleConnectionInterval_msec(getConnectionInterval_msec());
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
  Serial.print(F("nRF52840 Firmware: disconnect_callback: Disconnected, reason = 0x")); Serial.print(reason, HEX);
  Serial.print(F(", bleConnected = ")); Serial.println(bleConnected);
}


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
  if ((preset_id > 0) && (preset_id < MAX_N_PRESET_SERVICES)) {  //be sure to exclude preset_id 0 (never disable preset_id 0 because it's the DFU)
    return flag_activateServicePreset[preset_id] = enable;
  }
  return false;
}

int setAdvertisingServiceToPresetById(int preset_id) {
  if ((preset_id > 0) && (preset_id < MAX_N_PRESET_SERVICES)) {
      service_preset_to_ble_advertise = preset_id;

      //in case this function gets called after the system is running (or about to begin()), follow through with the next steps, too
      BLE_Service_Preset* service_ptr = activated_service_presets[service_preset_to_ble_advertise];
      if (service_ptr) serviceToAdvertise = service_ptr->getServiceToAdvertise();
      return service_preset_to_ble_advertise;
  }
  return -1;
}

void beginAllBleServices(int setup_config_id) {
  int preset_id;

  bleBegun = true;
  // set the MAC address
  if (!was_MAC_set_by_user) this_gap_addr.addr_type=BLE_GAP_ADDR_TYPE_PUBLIC; //there is probably a better place to ensure this is set...but I'm doing it here
  Bluefruit.setAddr(&this_gap_addr);

  //set the unique (has 16 HEX characters)!
  if (DEBUG_VIA_USB) { Serial.print("nRF52840 Firmware: begin: setting BLE name: "); Serial.println(deviceName); };
  Bluefruit.setName(deviceName);
  
  //setup the connect and disconnect callbacks
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);

  // To be consistent OTA DFU should be added first if it exists
  preset_id = 0;
  bledfu.begin(); // makes it possible to do OTA DFU

  // Configure and begin all of the other services (as requested)
  for (preset_id = 1; preset_id < MAX_N_PRESET_SERVICES; preset_id++) {  //start at 1, assuming 0 is always the dfu service
    if (flag_activateServicePreset[preset_id]) {
      if (DEBUG_VIA_USB) { Serial.print("nRF52840 Firmware: begin: starting preset service_id = "); Serial.println(preset_id); };
      err_t err_code = all_service_presets[preset_id]->begin(preset_id); 
      if (err_code != 0) {
        if (DEBUG_VIA_USB) Serial.print("nRF52840 Firmware: begin: service_id = " + String(preset_id) + " begin ERROR: code = " + String(err_code)); 
      }
      activated_service_presets[preset_id] = all_service_presets[preset_id];
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

  //set the device information
  ble_bleDis.setManufacturer(manufacturerName); //"Flywheel Lab");
  ble_bleDis.setModel(versionString);

  //collect all services
  for (int i=0; i<MAX_N_PRESET_SERVICES; i++) {
    all_service_presets[i] = nullptr;
    activated_service_presets[i] = nullptr;
    flag_activateServicePreset[i] = false;
  }
  int i;
  //i=0; all_service_presets[i] = &ble_bleDfu;       flag_activateServicePreset[i] = true;      //always keep activated!...actually, don't even put dfu in the list
  i=1; all_service_presets[i] = &ble_bleDis;       flag_activateServicePreset[i] = true;      //activate by default
  i++; all_service_presets[i] = &bleUart_Tympan;   flag_activateServicePreset[i] = true;  service_preset_to_ble_advertise = i; //advertise this one by default
  i++; all_service_presets[i] = &bleUart_Adafruit; flag_activateServicePreset[i] = true;      //activate by default
  i++; all_service_presets[i] = &ble_battService;  flag_activateServicePreset[i] = false;     //not active by default
  i++; all_service_presets[i] = &ble_lbs;          flag_activateServicePreset[i] = false;     //not active by default
  i++; all_service_presets[i] = &ble_lbs_4bytes;   flag_activateServicePreset[i] = false;     //not active by default
  i++; all_service_presets[i] = &ble_generic1;   flag_activateServicePreset[i] = false;     //not active by default
  i++; all_service_presets[i] = &ble_generic2;   flag_activateServicePreset[i] = false;     //not active by default

  
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

  was_MAC_set_by_user = true;

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
    BLE_Service_Preset *service_ptr = activated_service_presets[Iservice];
    if (!service_ptr) {
      //this is not a valid service pointer
      //Serial.println("sendBleDataByServiceAndChar: comparing given service_id " + String(service_id) + " to UNINITIALIZED preset service");
    } else {
      //this is a valid service pointer
      //Serial.println("sendBleDataByServiceAndChar: comparing given service_id " + String(service_id) + " to preset service " + String(service_ptr->service_id));
      if (service_ptr->service_id == service_id) {
        if (command == 1) {
          if (DEBUG_VIA_USB) {
            Serial.print(F("sendBleDataByServiceAndChar: BLE WRITE to characteristic ")); Serial.print(char_id); Serial.print(F(", data = "));
            Serial.write(databytes, nbytes);
            Serial.println();
          }
          service_ptr->write(char_id, databytes,nbytes); data_sent = true;
        } else if (command == 2) {
          if (DEBUG_VIA_USB) {
            Serial.print(F("sendBleDataByServiceAndChar: BLE NOTIFY to characteristic ")); Serial.print(char_id); Serial.print(F(", data = "));
            Serial.write(databytes, nbytes); 
            Serial.println(); 
          }
          service_ptr->notify(char_id, databytes,nbytes);data_sent = true;
        }
      }
    }
    Iservice++;  //increment to look at next service in the list
  }
  if ((data_sent == false) && DEBUG_VIA_USB) { Serial.print(F("sendBleDataByServiceAndChar: data NOT sent to service ")); Serial.print(service_id); Serial.println(F(" because no matching service ID found")); }
  if (data_sent == false) return -1;
  return 0;
}

//UUID is composed of 16 uint8_t numbers.  Given 32 characters, interpret as 16 uint8_t values.
//We also need to reverse each hex number, which means that we need to step backwards across pairs of characters
err_t interpretUUIDinReverse(const char *given_uuid, const int len_given_uuid_chars, UUID_t *targ_uuid) {
  int n_vals = (int)(len_given_uuid_chars/2); 
  if (n_vals != (targ_uuid->len)) return (err_t)1; //error, sizes are wrong!
  int source_ind, targ_ind;
  for (int Ival = 0; Ival < n_vals; ++Ival) {
    source_ind = (n_vals-Ival-1)*2; //end, moving backwards
    char c1 = given_uuid[source_ind];
    char c2 = given_uuid[source_ind+1];
    targ_uuid->uuid[Ival] = hexCharsToByte(c1,c2);
  }
  return (err_t)0;  //no error
}

err_t setServiceUUID(const int ble_service_id, const char *uuid_chars, const int len_uuid_chars) {
  //copy the UUID (in reverse order, as required by BLE_GenericService)
  UUID_t this_uuid;
  err_t err_code = interpretUUIDinReverse(uuid_chars, len_uuid_chars, &this_uuid);
  if (err_code !=0) return (err_t)1;  //error, wrong size

  //if the service_id is valid for a ble_generic, set the UUID and allow it to be enabled
  BLE_GenericService *ble_generic = nullptr;
  if (ble_service_id == 7) {
    ble_generic = &ble_generic1;
  } else if (ble_service_id == 8) {
    ble_generic = &ble_generic2;
  } else {
    return (err_t)2;  //error didn't recognize the ble_service_id
  }

  //assuming that we have a valid pointer, go ahead and set the UUID and enable the preset
  if (ble_generic != nullptr) {
    ble_generic->setServiceUUID(this_uuid);
    enablePresetServiceById(ble_service_id, true);
    return (err_t)0; //no error
  }

  return (err_t)99;  //we should not get here.  unknown error
}

err_t setServiceName(const int ble_service_id, const String name) {
  //if the service_id is valid for a ble_generic, set the UUID and allow it to be enabled
  BLE_GenericService *ble_generic = nullptr;
  if (ble_service_id == 7) {
    ble_generic = &ble_generic1;
  } else if (ble_service_id == 8) {
    ble_generic = &ble_generic2;
  } else {
    return (err_t)2;  //error didn't recognize the ble_service_id
  }

  //assuming that we have a valid pointer, go ahead and set the service name
  if (ble_generic != nullptr) {
    ble_generic->setServiceName(name);
    return (err_t)0; //no error
  }
  return (err_t)99;  //we should not get here.  unknown error
}

err_t addCharacteristic(const int ble_service_id, const char *uuid_chars, const int len_uuid_chars) {
  //copy the UUID (in reverse order, as required by BLE_GenericService)
  UUID_t this_uuid;
  err_t err_code = interpretUUIDinReverse(uuid_chars, len_uuid_chars, &this_uuid);
  if (err_code !=0) return (err_t)1;  //error, wrong size

  //if the service_id is valid for a ble_generic, set the UUID and allow it to be enabled
  BLE_GenericService *ble_generic = nullptr;
  if (ble_service_id == 7) {
    ble_generic = &ble_generic1;
  } else if (ble_service_id == 8) {
    ble_generic = &ble_generic2;
  } else {
    return (err_t)2;  //error didn't recognize the ble_service_id
  }

  //assuming that we have a valid pointer, go ahead and set the UUID and enable the preset
  if (ble_generic != nullptr) {
    err_code = ble_generic->addCharacteristic(this_uuid);
    if (err_code != 0) return (err_t)3; //could not create characteristic
    return (err_t)0; //no error
  }

  return (err_t)99;  //we should not get here.  unknown error
}

err_t setCharacteristicName(const int ble_service_id, const int ble_char_id, const String &name) {
  //if the service_id is valid for a ble_generic, set the UUID and allow it to be enabled
  BLE_GenericService *ble_generic = nullptr;
  if (ble_service_id == 7) {
    ble_generic = &ble_generic1;
  } else if (ble_service_id == 8) {
    ble_generic = &ble_generic2;
  } else {
    return (err_t)2;  //error didn't recognize the ble_service_id
  }

  //assuming that we have a valid pointer, go ahead and set the service name
  if (ble_generic != nullptr) {
    err_t err_code = ble_generic->setCharacteristicName(ble_char_id, name);
    if (err_code != 0) return (err_t)3; //could not set its name (char_id doesn't exist?)
    return (err_t)0; //no error
  }
  return (err_t)99;  //we should not get here.  unknown error
}

err_t setCharacteristicProps(const int ble_service_id, const int ble_char_id, const uint8_t char_props) {
  //if the service_id is valid for a ble_generic, set the UUID and allow it to be enabled
  BLE_GenericService *ble_generic = nullptr;
  if (ble_service_id == 7) {
    ble_generic = &ble_generic1;
  } else if (ble_service_id == 8) {
    ble_generic = &ble_generic2;
  } else {
    return (err_t)2;  //error didn't recognize the ble_service_id
  }

  //assuming that we have a valid pointer, go ahead and set the service name
  if (ble_generic != nullptr) {
    err_t err_code = ble_generic->setCharacteristicProps(ble_char_id, char_props);
    if (err_code != 0) return (err_t)3; //could not set its name (char_id doesn't exist?)
    return (err_t)0; //no error
  }
  return (err_t)99;  //we should not get here.  unknown error 
}

err_t setCharacteristicNBytes(const int ble_service_id, const int ble_char_id, const int n_bytes) {
  //if the service_id is valid for a ble_generic, set the UUID and allow it to be enabled
  BLE_GenericService *ble_generic = nullptr;
  if (ble_service_id == 7) {
    ble_generic = &ble_generic1;
  } else if (ble_service_id == 8) {
    ble_generic = &ble_generic2;
  } else {
    return (err_t)2;  //error didn't recognize the ble_service_id
  }

  //assuming that we have a valid pointer, go ahead and set the service name
  if (ble_generic != nullptr) {
    err_t err_code = ble_generic->setCharacteristicNBytes(ble_char_id, n_bytes);
    if (err_code != 0) return (err_t)3; //could not set its name (char_id doesn't exist?)
    return (err_t)0; //no error
  }
  return (err_t)99;  //we should not get here.  unknown error 
}





