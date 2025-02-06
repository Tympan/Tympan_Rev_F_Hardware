// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code will run on the nRF52 to enable the standard BLE battery service
//
// Created: Chip Audette Feb 2025
// MIT License.  Use at your own risk.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BLE_BattService_h
#define _BLE_BattService_h

#include <bluefruit.h>
#include "BLE_Service_Preset.h"

//define services and characteristics for a pre-set available to be invoked by the Tympan user at startup
class BLE_BattService : public virtual BLEBas, public virtual BLE_Service_Preset {
  public:
    BLE_BattService(void) : BLEBas(), BLE_Service_Preset() {};
    ~BLE_BattService(void) override {};
  
    err_t begin(int id) override  //err_t is inhereted from bluefruit.h?
    {
      BLE_Service_Preset::begin(id); //sets service_id
      return BLEBas::begin();
    }

    //additional methods required by BLE_Service_Preset
    size_t write( const int char_id, const uint8_t* data, size_t len) override { bool ret_val = BLEBas::write((uint8_t)data[0]); return (size_t)ret_val; };
    size_t notify(const int char_id, const uint8_t* data, size_t len) override { bool ret_val = BLEBas::notify((uint8_t)data[0]); return (size_t)ret_val; };
    BLEService* getServiceToAdvertise(void) override { return this; }

    //define how many characteristics and what their ID numbes are
    const int nchars = 1;  //max number of characteristics
    const int char_ids[1]  = {0};  //characteristic ids.  default.  might get overwritten
};

#endif
  