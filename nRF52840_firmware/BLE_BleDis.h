// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code will run on the nRF52 to enable the standard BLE Device Information Service (wrapper around the 
// BLEDis.h from Adafruit)
//
// Created: Chip Audette Feb 2025
// MIT License.  Use at your own risk.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _BLE_BleDis_h
#define _BLE_BleDis_h

#include <bluefruit.h>
#include "BLE_Service_Preset.h"

//define services and characteristics for a pre-set available to be invoked by the Tympan user at startup
class BLE_BleDis : public virtual BLEDis, public virtual BLE_Service_Preset {
  public:
    BLE_BleDis(void) : BLEDis(), BLE_Service_Preset() {
      name = "Device Information Service";
    };
    ~BLE_BleDis(void) override {};
  
    err_t begin(int id) override  //err_t is inhereted from bluefruit.h?
    {
      BLE_Service_Preset::begin(id); //sets service_id
      return BLEDis::begin();
      hasBegun = true;
    }

    //additional methods required by BLE_Service_Preset
    size_t write( const int characteristic_id, const uint8_t* data, size_t len) override { return notify(characteristic_id, data, len); };
    size_t notify(const int characteristic_id, const uint8_t* data, size_t len) override { 
      size_t ret_val = len;
      switch (characteristic_id) {
        case 0:
          setSystemID((char *)data, (uint8_t) len);
          break;
        case 1:
          setModel((char *)data, (uint8_t) len);
          break;
        case 2:
          setSerialNum((char *)data, (uint8_t) len);
          break;
        case 3:
          setFirmwareRev((char *)data, (uint8_t) len);
          break;
        case 4:
          setHardwareRev((char *)data, (uint8_t) len);
          break;
        case 5:
          setSoftwareRev((char *)data, (uint8_t) len);
          break;
        case 6:
          setManufacturer((char *)data, (uint8_t) len);
          break;
        case 7:
          setRegCertList((char *)data, (uint8_t) len);
          break;
        case 8:
          setPNPID((char *)data, (uint8_t) len);
          break;
        default:
          ret_val = 0;
          break;
      }

      //restart the servicewith the new info
      if (hasBegun) {
        if (ret_val != 0) BLEDis::begin();
      }

      return ret_val;
   }

    BLEService* getServiceToAdvertise(void) override { return this; }

    //define how many characteristics and what their ID numbes are
    const int nchars = 9;  //max number of characteristics
    const int char_ids[9]  = {0,1,2,3,4,5,6,7,8};  //characteristic ids.  default.  might get overwritten

    protected:
      bool hasBegun = false;

};

#endif