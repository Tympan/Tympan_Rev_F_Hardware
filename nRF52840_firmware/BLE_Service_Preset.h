/* 
  Class: BLE_Service_Preset
  Created: Chip Audette, OpenAudio  Feb 2025
  Purpose: Define a common interface for classes holding a complete definish of BLE Service
           plus associated BLE characteristics

  License: MIT License, Use at your own risk
*/

#ifndef _BLE_Service_Preset
#define _BLE_Service_Preset

#ifndef STD_VECTOR__throw_length_error
#include <vector>
namespace std {void __throw_length_error(char const*){ while(1); }; } //to get rid of a compile error assciated with vectors
#define __throw_length_error
#endif

extern void globalWriteBleDataToTympan(const int service_id, const int char_id, uint8_t data[], size_t len);

class BLE_Service_Preset {
  public:
    BLE_Service_Preset(void) {}
    virtual ~BLE_Service_Preset(void) {};
    virtual err_t begin(void) { return begin(0); }
    virtual err_t begin(int id) { service_id = id; has_begun = true; return (err_t)0; }
    virtual BLEService* getServiceToAdvertise(void) { return nullptr; }
    virtual size_t write( const int char_id, const uint8_t* data, size_t len) { return 0; }; //do nothing by default 
    virtual size_t notify(const int char_id, const uint8_t* data, size_t len) { return 0; }; //do nothing by default
    virtual String& getName(String &s) { s.remove(0,s.length()); return s += name; }

    static void writeBleDataToTympan(const int service_id, const int char_id, uint8_t data[], size_t len) { globalWriteBleDataToTympan( service_id, char_id, data, len); }

    int service_id = 0; //will get overwritten when actually setup
    bool has_begun = false;
    
    String name = "(no name)";
};

#endif
