// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code will run on the nRF52 to enable generic Write or Notify characteristics
//
// Created: Chip Audette Mar 2025
// MIT License.  Use at your own risk.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BLE_GenericService_h
#define _BLE_GenericService_h

// How to create a custom service:
// 1) Instantiate this class by providing a Service UUID via the constructor
// 2) [Optional] using setServiceName(), provide a human-readable name for the service
// 3) Define all characteristics via repeated calls to addCharacteristic().  Provide all info for a Characteristic via a BLE_CHAR_t structure.
// 4) After definin all characteristics, call begin()


#include <bluefruit.h>
#include "BLE_Service_Preset.h"
#include <vector>

//types to help setup a generic BLE service
typedef struct { 
  uint8_t uuid[16] = {0}; //store UUID bytes in reverse order
  unsigned int len = 16; 
  } UUID_t;  
typedef struct {
  UUID_t uuid;
  uint8_t n_bytes = 1; //bytes to be transmitted via this characteristic.  1 to 4?  more?
  uint8_t props = CHR_PROPS_NOTIFY | CHR_PROPS_READ | CHR_PROPS_WRITE;  //see Adafruit nRF52 library for all options
  String name;
} BLE_CHAR_t;

class BLE_GenericService : public virtual BLE_Service_Preset {
  public:
    BLE_GenericService(UUID_t new_uuid) : BLE_Service_Preset() {
      //copy the uuid locally
      for (auto i=0; i< new_uuid.len; ++i) ServiceUUID.uuid[i]=new_uuid.uuid[i];
      //create the new service
      this_service = new BLEService(ServiceUUID.uuid);
    }
    ~BLE_GenericService(void) override {
      //remove this instance from the static table holding pointers to all instances of BLE_GenericService
      for (auto i=0; i<self_ptr_table.size(); ++i) { if (self_ptr_table[i] == this) self_ptr_table[i] = nullptr;  }

      //delete all characteristics
      for (auto i=characteristic_ptr_table.size()-1; i >= 0; --i) delete characteristic_ptr_table[i];

      //delete characteristic info
      for (auto i=characteristic_info_table.size()-1; i>=0; --i) delete characteristic_info_table[i];

      //delete other pointers
      delete this_service;
    }

    void setServiceName(const String &new_name) {
      name.remove(0,name.length());  //clear out any existing name
      name += new_name;              //add the new name
    }

    err_t addCharacteristic(const BLE_CHAR_t &given_char) {
      if (characteristic_info_table.size() >= max_char_ids) return (err_t)1;  //can't create an excessive number of characteristics
      //instantiate
      BLE_CHAR_t *new_char_info = new BLE_CHAR_t;
      //copy data
      new_char_info->uuid = given_char.uuid;
      new_char_info->n_bytes = given_char.n_bytes;
      new_char_info->props = given_char.props;
      new_char_info->name = given_char.name;
      //push onto vector until it's needed
      characteristic_info_table.push_back(new_char_info); //To-do: get the return type and generate an error, if needed
      return (err_t)0;
    }

    err_t begin(int id) override {  //err_t is inhereted from bluefruit.h?
      BLE_Service_Preset::begin(id); //sets service_id

      // Note: You must call .begin() on the BLEService before calling .begin() on
      // any characteristic(s) within that service definition.. Calling .begin() on
      // a BLECharacteristic will cause it to be added to the last BLEService that
      // was 'begin()'ed!
      this_service->begin();

      // Configure each characteristic that has been defined
      for (auto i=0; i<characteristic_info_table.size(); ++i) {
        if ((int)characteristic_ptr_table.size() >= max_char_ids) return (err_t)1;  //attempting to create too many!

        //get the pre-defined info about this new characteristic
        BLE_CHAR_t *char_info = characteristic_info_table[i];

        //instantiate the new characteristic
        BLECharacteristic *new_char = new BLECharacteristic(char_info->uuid.uuid);
        if (new_char == nullptr) return (err_t)2;  //failed to create characteristics

        //set all of the properties of he new characteristic
        new_char->setProperties(char_info->props);
        new_char->setPermission(SECMODE_OPEN, SECMODE_OPEN);
        new_char->setFixedLen(char_info->n_bytes);
        new_char->setUserDescriptor(char_info->name.c_str());
        new_char->begin();
        if (char_info->props & CHR_PROPS_WRITE) { //can this charcterisitc receive data in?
          new_char->setWriteCallback(BLE_GenericService::write_callback); //must be a static function
        }

        //save this characteristic in our internal tracking table
        characteristic_ptr_table.push_back(new_char);
        nchars = (int)i;
        char_ids[(int)i]=(int)i;
      }
      return (err_t)0;
    }

    BLEService* getServiceToAdvertise(void) override { return this_service;  }

    size_t write(const int char_id, const uint8_t* data, size_t len) override {
      //reverse the bytes
      //uint8_t rev_data[len];
      //for (int I=0; I<len; I++) rev_data[len-I-1] = data[I];
      //
      //send to the correct characteristic
      if ((char_id >= 0) && (char_id < characteristic_ptr_table.size())) {
        BLECharacteristic *ble_char = characteristic_ptr_table[char_id];
        if (ble_char != nullptr) {
          return ble_char->write(data,len);
        }
      }
      return 0;
    }

    size_t notify(const int char_id, const uint8_t* data, size_t len) override {
      //reverse the bytes
      //uint8_t rev_data[len];
      //#for (int I=0; I<len; I++) rev_data[len-I-1] = data[I];
      //
      //send to the correct characteristic
      if ((char_id >= 0) && (char_id < characteristic_ptr_table.size())) {
        BLECharacteristic *ble_char = characteristic_ptr_table[char_id];
        if (ble_char != nullptr) {
          return ble_char->notify(data,len);
        }
      }
      return 0;
    }

    static void write_callback(uint16_t conn_hdl, BLECharacteristic* chr, uint8_t* data, uint16_t len);

    //types and memebers for defining a service and characteristic
    UUID_t ServiceUUID;

    //how many characterisitcs chan this service have?
    constexpr static int max_char_ids = 8; //maximum number
    int nchars = 0;  //current number
    int char_ids[max_char_ids]  = {0};   //default.  might get overwritten
    std::vector<BLE_CHAR_t *>characteristic_info_table;
    std::vector<BLECharacteristic *>characteristic_ptr_table; //here is where we'll store all the BLE characteristics (Adafruit nRF52 library) that we create
    BLEService *this_service; //here is the Adafruit nRF52 functionality that handles all the actual BLE interactions

    static std::vector<BLE_GenericService *> self_ptr_table; //all instances of this class....for use in the write callback

  protected:

};


#endif