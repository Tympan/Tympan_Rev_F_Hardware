/*
 *  Serial port control and communication
 */

 void serialEvent(){

    if(Serial.available()){
      char inChar = Serial.read();
      switch(inChar){
        case 'a':
          Serial.println("Stop fading");
          LEDsOff();
          ledToFade = -1;
          break;
        case 'r':
          Serial.println("Fade red LED 0");
          LEDsOff();
          ledToFade = red;
          fadeValue = FADE_MIN;
          break;
       case 'g':
          Serial.println("Fade green LED 1");
          LEDsOff();
          ledToFade = green;
          fadeValue = FADE_MIN;
          
          break;
       case 'b':
          Serial.println("Fade blue LED 2");
          LEDsOff();
          ledToFade = blue;
          fadeValue = FADE_MIN;

          break;
        case '?':
          printHelp();
          break;
        default:
      	  Serial.print(inChar); Serial.print(" not recognised\n");
          break;
      }
    }
 }

void printHelp(){
  if(usingSerial){
    Serial.print("\nTympanRadio_baseFirmware.ino ");
    Serial.println(versionString);
    Serial.print("BLE Address: "); Serial.println(deviceName);
    Serial.println("Serial Keyboard Commands");
    Serial.println("Send 'a' to stop fading");	
    Serial.println("Send 'r' to fade red LED 0");	
    Serial.println("Send 'g' to fade green LED 1");
    Serial.println("Send 'b' to fade blue LED 2");
    Serial.println("Press '?' to print this help");
    Serial.print("Please use Bluetooth to connect to ");
    Serial.println(deviceName);
    Serial.println("Once connected, use bleuart and send '?' to print help");
  }
  printedHelp = true;
}


