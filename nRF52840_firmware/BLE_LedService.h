// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code will run on the **nRF52** to allow the TympanRemote app see the UART-like service that we're offering
// over BLE.  This code will likely be part of our nRF52 firmware.
//
// Created: Chip Audette Feb 2025
// MIT License.  Use at your own risk.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BLE_LedService_h
#define _BLE_LedService_h

#include <bluefruit.h>
#include "BLE_Service_Preset.h"
//include <functional>
#include <vector>

class BLE_LedButtonService : public virtual BLE_Service_Preset {
  public:
    BLE_LedButtonService(void) : BLE_Service_Preset() {
      name = "LED Button Service (Nordic)";
      lbs = new BLEService(LBS_UUID_SERVICE);                  self_ptr_table.push_back(this);
      lbsButton = new BLECharacteristic(LBS_UUID_CHR_BUTTON);  characteristic_ptr_table.push_back(lbsButton);
      lbsLED = new BLECharacteristic(LBS_UUID_CHR_LED);        characteristic_ptr_table.push_back(lbsLED);

      char1_name += "Button";
      char2_name += "LED";
    }
    ~BLE_LedButtonService(void) override {
      delete lbsLED;  delete lbsButton;   delete lbs;
      //TBD: remove the service from the static table self_ptr_table
    }
    err_t begin(int id) override {  //err_t is inhereted from bluefruit.h?
      BLE_Service_Preset::begin(id); //sets service_id

      // Note: You must call .begin() on the BLEService before calling .begin() on
      // any characteristic(s) within that service definition.. Calling .begin() on
      // a BLECharacteristic will cause it to be added to the last BLEService that
      // was 'begin()'ed!
      lbs->begin();

      // Configure Button characteristic...updated by Chip Audette
      // Properties = Boradcast + Notify + Read
      // Permission = Open to read, Open to write
      // Fixed Len  = 4 (bytes...per screenshot of our own App)
      lbsButton->setProperties(char1_props);
      lbsButton->setPermission(SECMODE_OPEN, SECMODE_OPEN );  //allow to read, allow to write
      lbsButton->setFixedLen(nbytes_per_characteristic);
      lbsButton->setUserDescriptor(char1_name.c_str());
      lbsButton->begin();
      if (char1_props & (CHR_PROPS_NOTIFY | CHR_PROPS_READ)) { //can this characteristic send data out?
        if (nbytes_per_characteristic == 1) {
          lbsButton->write8(0x01);  //init value.  Appears in NordicConnect in the reverse byte order of shown here
        } else if  (nbytes_per_characteristic == 2) { 
          lbsButton->write16(0x0102);  //init value.  Appears in NordicConnect in the reverse byte order of shown here
        } else if (nbytes_per_characteristic == 4) {
          lbsButton->write32(0x00010203);  //init value.  Appears in NordicConnect in the reverse byte order of shown here
        }
      }

      // Configure the LED characteristic...updated by Chip Audette
      // Properties = Boradcast + Notify + Ready
      // Permission = Open to read, Open to write
      // Fixed Len  = 4 (bytes...per screenshot of our own App)
      lbsLED->setProperties(char2_props);
      lbsLED->setPermission(SECMODE_OPEN, SECMODE_OPEN );  //allow to read, allow to write
      lbsLED->setFixedLen(nbytes_per_characteristic);
      lbsLED->setUserDescriptor(char2_name.c_str());
      lbsLED->begin();
      if (char2_props & (CHR_PROPS_NOTIFY | CHR_PROPS_READ)) { //can this characteristic send data out?
        if (nbytes_per_characteristic == 1) {
          lbsLED->write8(0x01);  //init value.  Appears in NordicConnect in the reverse byte order of shown here
        } else if  (nbytes_per_characteristic == 2) { 
          lbsLED->write16(0x0405);  //init value.  Appears in NordicConnect in the reverse byte order of shown here
        } else if (nbytes_per_characteristic == 4) {
          lbsLED->write32(0x00040506);  //init value.  Appears in NordicConnect in the reverse byte order of shown here
        }
      }
      if (char2_props & CHR_PROPS_WRITE) { //can this charcterisitc receive data in?
        lbsLED->setWriteCallback(BLE_LedButtonService::led_write_callback); //must be a static function
      }
      return (err_t)0;
    }
    BLEService* getServiceToAdvertise(void) override {
      return lbs;
    }

    size_t write(const int char_id, const uint8_t* data, size_t len) override {
      //reverse the bytes
      //uint8_t rev_data[len];
      //for (int I=0; I<len; I++) rev_data[len-I-1] = data[I];
      //send to the correct characteristic
      if (char_id == 0) {
        return lbsButton->write(data,len);
      } else if (char_id == 1) {
        return lbsLED->write(data,len);
      }
      return 0;
    }

    size_t notify(const int char_id, const uint8_t* data, size_t len) override {
      //reverse the bytes
      //uint8_t rev_data[len];
      //#for (int I=0; I<len; I++) rev_data[len-I-1] = data[I];

      //send to the correct characteristic
      if (char_id == 0) {
        return lbsButton->notify(data,len);
      } else if (char_id == 1) {
        return lbsLED->notify(data,len);
      }
      return 0;
    }

    static void led_write_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);

        
    /* Nordic Led-Button Service (LBS)LBS_UUID_SERVICE
    * LBS Service: 00001523-1212-EFDE-1523-785FEABCD123
    * LBS Button : 00001524-1212-EFDE-1523-785FEABCD123
    * LBS LED    : 00001525-1212-EFDE-1523-785FEABCD123
    */

    const uint8_t LBS_UUID_SERVICE[16] =  //reverse order
    {
      0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
      0xDE, 0xEF, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00
    };

    const uint8_t LBS_UUID_CHR_BUTTON[16] = //reverse order
    {
      0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
      0xDE, 0xEF, 0x12, 0x12, 0x24, 0x15, 0x00, 0x00
    };

    const uint8_t LBS_UUID_CHR_LED[16] = //reverse order
    {
      0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
      0xDE, 0xEF, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00
    };

    uint8_t nbytes_per_characteristic = 1;
    const int max_char_ids = 8;
    int nchars = 2;
    int char_ids[8]  = {0};  //default.  might get overwritten
    uint8_t char1_props = CHR_PROPS_NOTIFY | CHR_PROPS_READ;
    uint8_t char2_props = CHR_PROPS_WRITE;
    BLEService *lbs;
    BLECharacteristic *lbsButton;
    BLECharacteristic *lbsLED;
    String char1_name;
    String char2_name;
    static std::vector<BLE_LedButtonService *> self_ptr_table;
    std::vector<BLECharacteristic *>characteristic_ptr_table;
  private:

};

//initialize the static variables
std::vector<BLE_LedButtonService*> BLE_LedButtonService::self_ptr_table = {};

//define services and characteristics for a pre-set available to be invoked by the Tympan user at startup
class BLE_LedButtonService_4bytes : public virtual BLE_LedButtonService {
  public:
    BLE_LedButtonService_4bytes(void) : BLE_LedButtonService() {
      lbsStartTest = new BLECharacteristic(LBS_UUID_CHR_STARTBUTTON);  
      characteristic_ptr_table.push_back(lbsStartTest);
      
      name = "LED Button Service (4-Byte)";
      nbytes_per_characteristic = 4;
      char1_name += " (4 bytes)";    char_ids[0] = 0;
      char2_name += " (4 bytes)";    char_ids[1] = 1;
      char3_name += "Flex (4 bytes)";char_ids[2]= 2;
      char1_props = CHR_PROPS_NOTIFY | CHR_PROPS_READ;
      char2_props = CHR_PROPS_NOTIFY | CHR_PROPS_READ;
      char3_props = CHR_PROPS_WRITE;
    }
    ~BLE_LedButtonService_4bytes(void) override { delete lbsStartTest; }

    const uint8_t LBS_UUID_CHR_STARTBUTTON[16] = //reverse order  of '00001526-1212-EFDE-1523-785FEABCD123'
    {
      0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
      0xDE, 0xEF, 0x12, 0x12, 0x26, 0x15, 0x00, 0x00
    };

    BLECharacteristic *lbsStartTest;
    String char3_name;
    uint8_t char3_props = CHR_PROPS_WRITE;

};


void BLE_LedButtonService::led_write_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len)
{
  int service_id = -1, char_id = -1;

  //find matching service UUID in the static table
  BLEService* svc= &chr->parentService();
  for (size_t I = 0; I < BLE_LedButtonService::self_ptr_table.size(); ++I) {
    if (((BLE_LedButtonService::self_ptr_table[I])->lbs)->uuid == svc->uuid) {
      //found the service pointer

      //Now, find matching chracteristic UUID in its table of characteristic ids
      BLE_LedButtonService* foo_ble_svc_ptr = BLE_LedButtonService::self_ptr_table[I];
      service_id = foo_ble_svc_ptr->service_id;
      for (size_t J=0; J < foo_ble_svc_ptr->characteristic_ptr_table.size(); ++J) {
        if ((foo_ble_svc_ptr->characteristic_ptr_table[J])->uuid == chr->uuid) {
          char_id = J;
        }
      }
      break;
    }
  }

  Serial.print("BLE_LedService: led_write_callback:");
  Serial.print(" Recevied value = " + String(data[0]));
  //Serial.print(", from: "); Serial.print(central_name);
  if (service_id >= 0) { 
    Serial.print(", from service_id: " + String(service_id));
  } else {
    Serial.print(", from service UUID: " ); Serial.print(svc->uuid.toString());
  }
  if (char_id >= 0) {
    Serial.print(", from char_id: " + String(char_id));
  } else {
    Serial.print(", from char UUID: "); Serial.print(chr->uuid.toString());
  }
  Serial.println();

  //push the data to the Tympan
  if ((service_id >= 0) && (char_id >= 0)) BLE_Service_Preset::writeBleDataToTympan(service_id, char_id, data, len);
}



#endif
