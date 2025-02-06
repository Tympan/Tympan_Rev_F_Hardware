#ifndef _BLE_LedService_h
#define _BLE_LedService_h

#include <bluefruit.h>
#include "BLE_Service_Preset.h"

//define services and characteristics for a pre-set available to be invoked by the Tympan user at startup
class BLE_LedButtonService_4bytes : public BLE_Service_Preset {
  public:
    BLE_LedButtonService_4bytes(void) : BLE_Service_Preset() {
      lbs = new BLEService(LBS_UUID_SERVICE);
      lbsButton = new BLECharacteristic(LBS_UUID_CHR_BUTTON);
      lbsLED = new BLECharacteristic(LBS_UUID_CHR_LED);
    }
    ~BLE_LedButtonService_4bytes(void) override {
      delete lbsLED;  delete lbsButton;   delete lbs;
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
      lbsButton->setProperties(CHR_PROPS_BROADCAST | CHR_PROPS_NOTIFY | CHR_PROPS_READ);
      lbsButton->setPermission(SECMODE_OPEN, SECMODE_OPEN );  //allow to read, allow to write
      lbsButton->setFixedLen(4);
      lbsButton->begin();
      lbsButton->write32(0x00010203);  //init value.  Appears in NordicConnect in the reverse byte order of shown here

      // Configure the LED characteristic...updated by Chip Audette
      // Properties = Boradcast + Notify + Ready
      // Permission = Open to read, Open to write
      // Fixed Len  = 4 (bytes...per screenshot of our own App)
      lbsLED->setProperties(CHR_PROPS_BROADCAST | CHR_PROPS_NOTIFY | CHR_PROPS_READ);
      lbsLED->setPermission(SECMODE_OPEN, SECMODE_OPEN );  //allow to read, allow to write
      lbsLED->setFixedLen(4);
      lbsLED->begin();
      lbsLED->write32(0x00040506);  //init value. Appears in NordicConnect in the reverse byte order of shown here

      // lbsLED->setWriteCallback(led_write_callback);
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

    // void led_write_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len)
    // {
    //   (void) conn_hdl;
    //   (void) chr;
    //   (void) len; // len should be 1

    //   // data = 1 -> LED = On
    //   // data = 0 -> LED = Off
    //   setLED(data[0]);
    // }

        
    /* Nordic Led-Button Service (LBS)LBS_UUID_SERVICE
    * LBS Service: 00001523-1212-EFDE-1523-785FEABCD123
    * LBS Button : 00001524-1212-EFDE-1523-785FEABCD123
    * LBS LED    : 00001525-1212-EFDE-1523-785FEABCD123
    */

    const uint8_t LBS_UUID_SERVICE[16] =
    {
      0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
      0xDE, 0xEF, 0x12, 0x12, 0x23, 0x15, 0x00, 0x00
    };

    const uint8_t LBS_UUID_CHR_BUTTON[16] =
    {
      0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
      0xDE, 0xEF, 0x12, 0x12, 0x24, 0x15, 0x00, 0x00
    };

    const uint8_t LBS_UUID_CHR_LED[16] =
    {
      0x23, 0xD1, 0xBC, 0xEA, 0x5F, 0x78, 0x23, 0x15,
      0xDE, 0xEF, 0x12, 0x12, 0x25, 0x15, 0x00, 0x00
    };

    const int nchars = 2;
    const int char_ids[2]  = {0,1};  //default.  might get overwritten
    BLEService *lbs;
    BLECharacteristic *lbsButton;
    BLECharacteristic *lbsLED;
  private:

};

#endif