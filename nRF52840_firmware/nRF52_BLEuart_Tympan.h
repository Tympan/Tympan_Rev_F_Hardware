
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
    BLEUart_Tympan(void) : BLEUart(), BLE_Service_Preset() {};
    ~BLEUart_Tympan(void) override {};

    virtual void setCharacteristicUuid(BLECharacteristic &ble_char)
    { 
      _txd = ble_char;  //_txd is in BLEUart
      _rxd = ble_char;  //_rxd is in BLEUart
    };

    err_t begin(int id) override  //err_t is inhereted from bluefruit.h?
    {
      BLE_Service_Preset::begin(id); //sets service_id

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

};

#endif