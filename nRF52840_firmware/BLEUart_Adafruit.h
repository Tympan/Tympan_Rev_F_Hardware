/* 
  Class: BLEUart_Adafruit
  Created: Chip Audette, OpenAudio  May 2025
  Purpose: Extend the Adafuit_nF52_Arduino/BLEUart class to add in the methods required by the
           BLE_Service_Preset interface

  License: MIT License, Use at your own risk
*/

#ifndef BLEUART_ADAFRUIT_h
#define BLEUART_ADAFRUIT_h

#include <bluefruit.h> //for BLUUart
#include "BLE_Service_Preset.h"

class BLEUart_Adafruit : public virtual BLEUart, public virtual BLE_Service_Preset
{
  public:
    BLEUart_Adafruit(void) : BLEUart(), BLE_Service_Preset() {
      name = "UART (Adafruit)";
    };
    ~BLEUart_Adafruit(void) override {};

    err_t begin(int id) override  //err_t is inhereted from bluefruit.h?
    {
      BLE_Service_Preset::begin(id); //sets service_id
      return BLEUart::begin();
    }

    //additional methods required by BLE_Service_Preset
    size_t write( const int char_id, const uint8_t* data, size_t len) override { return BLEUart::write(data, len); }
    size_t notify(const int char_id, const uint8_t* data, size_t len) override { return BLEUart::write(data, len); }
    BLEService* getServiceToAdvertise(void) override { return this; }

    //define how many characteristics and what their ID numbes are
    const int nchars = 1;  //max number of characteristics
    const int char_ids[1]  = {0};  //characteristic ids.  default.  might get overwritten

};

#endif