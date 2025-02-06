#ifndef _BLE_Service_Preset
#define _BLE_Service_Preset

/* 
  Class: BLE_Service_Preset
  Created: Chip Audette, OpenAudio  Feb 2025
  Purpose: Define a common interface for classes holding a complete definish of BLE Service
           plus associated BLE characteristics

  License: MIT License, Use at your own risk
*/

class BLE_Service_Preset {
  public:
    BLE_Service_Preset(void) {}
    virtual ~BLE_Service_Preset(void) {};
    virtual err_t begin(void) { return begin(0); }
    virtual err_t begin(int id) { service_id = id; return (err_t)0; }
    virtual BLEService* getServiceToAdvertise(void) { return nullptr; }
    virtual size_t write( const int char_id, const uint8_t* data, size_t len) { return 0; }; //do nothing by default 
    virtual size_t notify(const int char_id, const uint8_t* data, size_t len) { return 0; }; //do nothing by default

    int service_id = 0; //will get overwritten when actually setup
};

#endif
