/*
 *  BLEwrite()
 *    Main function that sends data over bluetooth
 *    Checks for open connection
 *    Can send up to 64 ascii characters
 */
int BLEwrite(){
  int success = 0;
  // airString += String('\n');
  int counter = 0;
  for(int i=0; i<OUT_STRING_LENGTH; i++){
		if(outString[i] == 0){ break; }
    BLEmessage[i] = outString[i];
		counter++;
  }
	BLEmessage[counter] = '\n';
	counter++;
  if(bleConnected){
    success = bleuart.write( BLEmessage, counter );
  }else{
    success = -1;
  }
  return success;
}

/*
 *  BLEwriteInt(int)
 *  concatinates the int with outString
 *  Then calles BLEwrite
 */
void BLEwriteInt(int i){
  sprintf(numberBuffer, "%d",i);
  strcat(outString,numberBuffer);
  BLEwrite();
}

/*
 *  BLEwriteFloat(String,float,int)
 *  Combines a string with a float and number of decimal places
 *  Then calles BLEwrite
 */
void BLEwriteFloat(float f,int dPlaces){
  sprintf(numberBuffer, "%.*f", dPlaces, f);
  strcat(outString,numberBuffer);
  BLEwrite();
}

// this gets called to check on incoming BLE data
int BLEevent(){
  // expect to get a single ascii character command
  int success = -1;
  if(bleuart.available()){
    success = 0;
    while (bleuart.available()){
      bleInChar = bleuart.read();
      bleInBuffer[bleBufCounter] = bleInChar;
      bleBufCounter++;
    }
    switch(bleInBuffer[0]){
      case 'a':
          strcpy(outString,"Stop fading"); BLEwrite();
          LEDsOff();
          ledToFade = -1;
          break;
       case 'r':
          strcpy(outString,"Fade Red"); BLEwrite();
          LEDsOff();
          ledToFade = LED_0;
          fadeValue = FADE_MIN;
          break;
       case 'g':
          strcpy(outString,"Fade Blue"); BLEwrite();
          LEDsOff();
          ledToFade = LED_1;
          fadeValue = FADE_MIN;
          break;
       case 'b':
          strcpy(outString,"Fade Green"); BLEwrite();
          LEDsOff();
          ledToFade = LED_2;
          fadeValue = FADE_MIN;
          break;

        case '?':
          printBLEhelp();
          break;
//         case '\n': case '\r':
//           // ignore line breaks and carriage returns if they are hanging out
//           break;
        default:
          outString[0] = bleInBuffer[0]; outString[1] = '\0';
          strcat(outString," is not recognized");
          BLEwrite();
          break;
    }
    bleBufCounter = 0;
  }
  return success;
}

void setupBLE(){
  // Disable pin 19 LED function. We don't use pin 19
  Bluefruit.autoConnLed(false);
  // Config the peripheral connection with maximum bandwidth
  // more SRAM required by SoftDevice
  // Note: All config***() function must be called before begin()
  Bluefruit.configPrphBandwidth(BANDWIDTH_MAX);
  // other config***() ??
  Bluefruit.begin();
  Bluefruit.setTxPower(4);    // Check bluefruit.h for supported values
  uniqueID = getMcuUniqueID();
  // unique ID has 16 HEX characters!
  deviceName[7] = uniqueID.charAt(11); // [10];
  deviceName[8] = uniqueID.charAt(12); // [11];
  deviceName[9] = uniqueID.charAt(13); // [12];
  deviceName[10] = uniqueID.charAt(14); // [13];
  Bluefruit.setName(deviceName);
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  // To be consistent OTA DFU should be added first if it exists
  bledfu.begin(); // makes it possible to do OTA DFU
  // Configure and Start Device Information Service
  bledis.setManufacturer(manufacturerName); //"Flywheel Lab");
  bledis.setModel(versionString);
  bledis.begin();
  // Configure and Start BLE Uart Service
  bleuart.begin();
}

void startAdv(void)
{
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  // Include bleuart 128-bit uuid
  Bluefruit.Advertising.addService(bleuart);
  // Add 'Name' to Advertising packet
  Bluefruit.ScanResponse.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   *
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
  Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
}

// callback invoked when central connects
void connect_callback(uint16_t conn_handle)
{
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);
  handle = conn_handle; // save this for when/if we disconnect
  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));
  if(usingSerial){ Serial.print("Connected to "); Serial.println(central_name); }
  bleConnected = true;
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) conn_handle;
  (void) reason;

  if(usingSerial){
    Serial.println(); Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
  }
  bleConnected = false;
}


void printBLEhelp(){
  if(bleConnected){
    strcpy(outString,versionString); BLEwrite();
    strcpy(outString,"BLE Address: "); strcat(outString,deviceName); BLEwrite();
    strcpy(outString,"BLE Key Commands"); BLEwrite();
    strcpy(outString,"Send 'a' to stop blinking"); BLEwrite();
    strcpy(outString,"Send 'r' to blink red LED 0"); BLEwrite();
    strcpy(outString,"Send 'g' to blink green LED 1"); BLEwrite();
    strcpy(outString,"Send 'b' to blink blue LED 2"); BLEwrite();
    strcpy(outString,"Send '?' to print this help"); BLEwrite();
  }
  printedBLEhelp = true;
}

