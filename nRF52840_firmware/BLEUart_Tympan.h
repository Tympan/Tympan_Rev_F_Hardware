
// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code will run on the **nRF52** to allow the TympanRemote app see the UART-like service that we're offering
// over BLE.  This code will likely be part of our nRF52 firmware.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef BLEUART_TYMPAN_h
#define BLEUART_TYMPAN_h

/* 
  Class: BLEUart_Tympan
  Created: Chip Audette, OpenAudio  Feb 2024
  Purpose: Extend the Adafuit_nF52_Arduino/BLEUart class to allow setting of the characteristic UUIDs to be the same.
      REV 2024-Aug: Combine the read and write properites into a single characteristic.

  License: MIT License, Use at your own risk
*/

#include <bluefruit.h> //for BLUUart
#include "BLE_Service_Preset.h"

class BLEUart_Tympan : public virtual BLEUart, public virtual BLE_Service_Preset
{
  public:
    BLEUart_Tympan(void) : BLEUart(), BLE_Service_Preset() {
       myBleChar = new BLECharacteristic(this_characteristicUUID, BLENotify | BLEWrite); //from #include <bluefruit.h>
    };
    ~BLEUart_Tympan(void) override { delete myBleChar; };

    virtual void setCharacteristicUuid(BLECharacteristic &ble_char)
    { 
      _txd = ble_char;  //_txd is in BLEUart
      _rxd = ble_char;  //_rxd is in BLEUart
    };

    err_t begin(int id) override  //err_t is inhereted from bluefruit.h?
    {
      BLE_Service_Preset::begin(id); //sets service_id

      setUuid(this_serviceUUID);
      setCharacteristicUuid(*myBleChar);
     
      _rx_fifo = new Adafruit_FIFO(1);
      _rx_fifo->begin(_rx_fifo_depth);

      // Invoke base class begin()
      VERIFY_STATUS( BLEService::begin() );

      uint16_t max_mtu = Bluefruit.getMaxMtu(BLE_GAP_ROLE_PERIPH);

      // // Add TXD Characteristic
      // _txd.setProperties(CHR_PROPS_NOTIFY);
      // // TODO enable encryption when bonding is enabled
      // _txd.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
      // _txd.setMaxLen( max_mtu );
      // _txd.setUserDescriptor("TXD");
      // VERIFY_STATUS( _txd.begin() );

      // // Add RXD Characteristic
      // _rxd.setProperties(CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
      // _rxd.setWriteCallback(BLEUart::bleuart_rxd_cb, true);
      // TODO enable encryption when bonding is enabled
      // _rxd.setPermission(SECMODE_NO_ACCESS, SECMODE_OPEN);
      // _rxd.setMaxLen( max_mtu );
      // _rxd.setUserDescriptor("RXD");
      // VERIFY_STATUS(_rxd.begin());

      // Add combiend TX / RX Characteristic
      _txd.setProperties(CHR_PROPS_NOTIFY | CHR_PROPS_WRITE | CHR_PROPS_WRITE_WO_RESP);
      _txd.setPermission(SECMODE_OPEN, SECMODE_OPEN);
      _txd.setMaxLen( max_mtu );
      _txd.setWriteCallback(BLEUart::bleuart_rxd_cb, true);
      _txd.setUserDescriptor("TXD_RXD");
      VERIFY_STATUS( _txd.begin() );


      return ERROR_NONE;
    }

    //additional methods required by BLE_Service_Preset
    size_t write( const int char_id, const uint8_t* data, size_t len) override { return BLEUart::write(data, len); }
    size_t notify(const int char_id, const uint8_t* data, size_t len) override { return BLEUart::write(data, len); }
    BLEService* getServiceToAdvertise(void) override { return this; }

    // ID Strings to be used by the nRF52 firmware to enable recognition by Tympan Remote App
    //String serviceUUID = String("BC-2F-4C-C6-AA-EF-43-51-90-34-D6-62-68-E3-28-F0");
    //String characteristicUUID = String("06-D1-E5-E7-79-AD-4A-71-8F-AA-37-37-89-F7-D9-3C");
    uint8_t this_serviceUUID[16] =        { 0xF0, 0x28, 0xE3, 0x68, 0x62, 0xD6, 0x34, 0x90, 0x51, 0x43, 0xEF, 0xAA, 0xC6, 0x4C, 0x2F, 0xBC }; //reverse order!
    uint8_t this_characteristicUUID[16] = { 0x3C, 0xD9, 0xF7, 0x89, 0x37, 0x37, 0xAA, 0x8F, 0x71, 0x4A, 0xAD, 0x79, 0xE7, 0xE5, 0xD1, 0x06 };  //reverse order!
    BLECharacteristic *myBleChar;

    //define how many characteristics and what their ID numbes are
    const int nchars = 1;  //max number of characteristics
    const int char_ids[1]  = {0};  //characteristic ids.  default.  might get overwritten

};

#endif
