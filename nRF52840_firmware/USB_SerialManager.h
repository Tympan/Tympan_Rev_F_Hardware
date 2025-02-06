
#ifndef USB_SERIALMANAGER_H
#define USB_SERIALMANAGER_H

extern bool bleBegun;
extern bool bleConnected;
extern const char versionString[];
extern void beginAllBleServices(int);
extern void issueATCommand(const char *, unsigned int);
extern void issueATCommand(const String&);
extern bool enablePresetServiceById(int preset_id, bool enable);
extern int setAdvertisingServiceToPresetById(int preset_id);


void printHelpToUSB(void) {
  Serial.println("nRF52840 Firmware: Help:");
  Serial.print(  "   : Version string: "); Serial.println(versionString);
  Serial.println(" : Status:");
  Serial.print(  "   : bleBegun: "); Serial.println(bleBegun);
  Serial.print(  "   : bleConnected: "); Serial.println(bleConnected);
  Serial.println("   : Send 'h' via USB to get this help");
  if (bleBegun == false) {
    Serial.println(" : Configuration:");
    Serial.println("   : Send 'M' to set MAC address to AABBCCEEDDFF");
    Serial.println("   : send '1' to enable Tympan UART Service (already enabled by default?");
    Serial.println("   : send '!' to enable Tympan UART as the Advertised Service");
    Serial.println("   : send '2' to enable Adafruit UART Service (already enabled by default?");
    Serial.println("   : send '@' to enable Adafruit UART as the Advertised Service");
    Serial.println("   : send '3' to enable LBS Service");
    Serial.println("   : send '#' to enable LBS as the Advertised Service");
    Serial.println("   : Send 'b' or 'B' to begin all enabled services");
  }
  Serial.println(" : Trial Commands:");
  Serial.println("   : Send 'J' via USB to send 'J' to the Tympan");
  Serial.println("   : Send 'q' to send AT command 'BLENOTIFY 1 0 1 9");
  Serial.println("   : Send 'w' to send AT command 'BLENOTIFY 2 0 2 aa");
  Serial.println("   : Send 'e' to send AT command 'BLENOTIFY 3 0 4 0x42C80000 (which is 100.0)");
  Serial.println("   : Send 'r' to send AT command 'BLENOTIFY 3 1 4 5678");
}

int serialManager_processCharacter(char c) {
  //Serial.print("serialManager_processCharacter: Received via USB: " + String(c));
  switch (c) {
    case 'h': case '?':
      printHelpToUSB();
      break;
    case 'b':
      Serial.println("nRF52840 Firmware: starting all BLE services (default)...");
      beginAllBleServices(1);
      break;
    case 'B':
      issueATCommand(String("SET BEGIN=1"));
      break;
    case 'M':
      issueATCommand(String("SET MAC=AABBCCDDEEFF"));
      break;


    case '1':
      Serial.println("nRF52840 Firmware: enabling preset " + String(c));
      enablePresetServiceById((int)(c - '0'),true);
      break;
    case '2':
      Serial.println("nRF52840 Firmware: enabling preset " + String(c));
      enablePresetServiceById((int)(c - '0'),true);
      break;
    case '3':
      Serial.println("nRF52840 Firmware: enabling preset " + String(c));
      enablePresetServiceById((int)(c - '0'),true);
      break;
    case '!':
      Serial.println("nRF52840 Firmware: choose preset 1 for advertising");
      setAdvertisingServiceToPresetById(1);
      break;
    case '@':
      Serial.println("nRF52840 Firmware: choose preset 2 for advertising");
      setAdvertisingServiceToPresetById(2);
      break;
    case '#':
      Serial.println("nRF52840 Firmware: choose preset 3 for advertising");
      setAdvertisingServiceToPresetById(3);
      break;  


    case 'J':
      Serial.println("nRF52840 Firmware: sending J to Tympan...");
      SERIAL_TO_TYMPAN.println("J");
      break;
    case 'q':
      issueATCommand(String("BLENOTIFY 1 0 1 9"));
      break;
    case 'w':
      issueATCommand(String("BLENOTIFY 2 0 2 aa"));
      break;
    case 'e':
      {
        const unsigned int len_msg = 20;
        char message[len_msg] = {'B','L','E','N','O','T','I','F','Y',' ','3',' ','0',' ','4',' ', 0x42, 0xC8, 0x00, 0x00};
        issueATCommand(message,len_msg);
      }
      break;
    case 'r':
      issueATCommand(String("BLENOTIFY 3 1 4 5678"));
      break;

  }
  return 0;
}

#endif