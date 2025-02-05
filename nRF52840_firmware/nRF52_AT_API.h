// ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// This code will run on the **nRF52** to interpret the AT-style commands coming in over the wired Serial1 link
// from the Tympan.   This code will likely be part of our nRF52 firmware.
//
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef nRF52_AT_API_H
#define nRF52_AT_API_H

#include <bluefruit.h>  //gives us the global "Bluefruit" class instance
#include "LED_controller.h"
extern LED_controller led_control;

//declared in BLE_Stuff.h or TympanBLE.h.  Should we move them inside here?  
extern bool bleBegun;
extern bool bleConnected;
extern char BLEmessage[];
extern void setMacAddress(char *);
extern void startAdv(void);
extern void stopAdv(void);
extern void beginAllBleServices(int);
extern const char versionString[];
extern void sendBleDataByServiceAndChar(int command, int service_id, int char_id, int nbytes, const uint8_t *databytes);

//running on the nRF52, this interprets commands coming from the hardware serial and send replies out to the BLE
#define nRF52_AT_API_N_BUFFER 512
class nRF52_AT_API {
  public:
    nRF52_AT_API(BLEUart_Tympan *_bleuart1, HardwareSerial *_ser_ptr) : ble_ptr1(_bleuart1) , serial_ptr(_ser_ptr) {}
    nRF52_AT_API(BLEUart_Tympan *_bleuart1, BLEUart *_bleuart2, HardwareSerial *_ser_ptr) : ble_ptr1(_bleuart1), ble_ptr2(_bleuart2), serial_ptr(_ser_ptr) {}
    
    virtual char getFirstCharInBuffer(void);
    virtual void addToSerialBuffer(char c);
    virtual int processSerialCharacter(char c);  //here's the main entry point to the AT message parsing
    virtual int processSerialCharacterAsBleMessage(char c);
    virtual int lengthSerialMessage(void);
    virtual int processSerialMessage(void);

  protected:
    BLEUart_Tympan *ble_ptr1 = NULL;
    BLEUart *ble_ptr2 = NULL;
    HardwareSerial *serial_ptr = &Serial1;
    char EOC = '\r'; //all commands (including "SEND") from the Tympan must end in this character
    enum RXMODE {RXMODE_LOOK_FOR_ANY = 0, 
                RXMODE_LOOK_FOR_CR_ONLY, 
                RXMODE_LOOK_FOR_COMMAND, 
                RXMODE_LOOK_FOR_SERVICE, 
                RXMODE_LOOK_FOR_CHARACTERISTIC, 
                RXMODE_LOOK_FOR_NBYTES,
                RXMODE_LOOK_FOR_DATABYTES};
    int rx_mode = RXMODE_LOOK_FOR_ANY;
    enum BLECOMMAND {BLECOMMAND_NONE=0, BLECOMMAND_WRITE, BLECOMMAND_NOTIFY};
    int ble_command = BLECOMMAND_NONE;
    int ble_service_id= 0;
    int ble_char_id = 0;
    int ble_nbytes = 0;
    //int ble_databyte_counter = 0;
    const int max_ble_nbytes = 32;
    uint8_t ble_databytes[32];

    //circular buffer for reading from Serial
    char serial_buff[nRF52_AT_API_N_BUFFER];
    int serial_read_ind = 0;
    int serial_write_ind = 0;

    //methods corresponding to the detailed actions that can be taken
    bool compareStringInSerialBuff(const char* test_str, int n);  
    int processSetMessageInSerialBuff(void);  
    int processGetMessageInSerialBuff(void);
    int setBeginFromSerialBuff(void);
    int setMacAddressFromSerialBuff(void);
    int setBleNameFromSerialBuff(void);
    int setAdvertisingFromSerialBuff(void);
    int setLedModeFromSerialBuff(void);
    int bleSendFromSerialBuff(void);
    //int bleNotifyFromSerialBuff(void);
    //int bleWriteFromSerialBuff(void);
    void debugPrintMsgFromSerialBuff(void);
    void debugPrintMsgFromSerialBuff(int, int);
    void sendSerialOkMessage(void);
    void sendSerialOkMessage(const char* reply_str);
    void sendSerialFailMessage(const char* reply_str);

    const int VERB_NOT_KNOWN = 1;
    const int PARAMETER_NOT_KNOWN = 2;
    const int FORMAT_PROBLEM = 3;
    const int OPERATION_FAILED = 4;
    const int NOT_IMPLEMENTED_YET = 5;
    const int DATA_WRONG_SIZE = 6;
    const int DATA_WRONG_FORMAT = 7;
    const int NO_BLE_CONNECTION = 99;
};

void nRF52_AT_API::addToSerialBuffer(char c) {
  serial_buff[serial_write_ind] = c; serial_write_ind++; if (serial_write_ind >= nRF52_AT_API_N_BUFFER) serial_write_ind = 0;
}

char nRF52_AT_API::getFirstCharInBuffer(void) {
  char c = serial_buff[serial_read_ind];
  int test_n_char = 1;
  serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
  return c;
}

//here's the main entry point to the AT message parsing
int nRF52_AT_API::processSerialCharacter(char c) {
  int test_n_char=0;
  //Serial.println("nRF52_AT_API::processSerialCharacter: rx_mode " + String(rx_mode) + ", char = " + String(c));

  if (rx_mode == RXMODE_LOOK_FOR_ANY) {
    //this branch will, at most, go only 3 characters.  If one is EOC, interpret it as required
    if (c == EOC) {  //look for the end-of-command character
      processSerialMessage();//the EOC character is NOT added to the serial_buff.  Just go ahead and interpret the serial_buff
    } else {
      addToSerialBuffer(c); //add the character to the buffer
      //look for the BLEWRITE or BLENOTIFY keywrds, which indicate byte-counting operations are needed
      if (lengthSerialMessage() == 3) {
        if (compareStringInSerialBuff("BLE",3) == true) {
          rx_mode = RXMODE_LOOK_FOR_COMMAND;
          ble_command = BLECOMMAND_NONE;
        } else {
          rx_mode = RXMODE_LOOK_FOR_CR_ONLY;
        }
      }
    }
  } else if (rx_mode == RXMODE_LOOK_FOR_CR_ONLY) {
    if (c == EOC) {  //look for the end-of-command character
      processSerialMessage();//the EOC character is NOT added to the serial_buff.  Just go ahead and interpret the serial_buff
    } else {
      addToSerialBuffer(c); //add the character to the buffer
    }
  } else {
    return processSerialCharacterAsBleMessage(c);
  } 
  return 0;
}


//"Noitfy" (and "Write") is for byte array payloads to be sent via BLE characteristic (not a UART-like service).
//The data bytes can have a carriage return character in the data payload.  It must also have a carriage return
// at the end of the serial buffer, though, marking the end of the overall message
//
//Format: NOTIFY X Y ZZ dddd
//Format: WRITE X Y ZZ dddd
//
//  X is one character (0-9) that is the id of the BLE service to employ for the transmission
//  Y is one character (0-9) that is the id of the BLE characteristic to employ for the transmission
//  Z is one character (1-9)) that is the number of bytes of the data
//  dddd are the bytes (1-9 bytes long) to be transmitted
int nRF52_AT_API::processSerialCharacterAsBleMessage(char c) {
  int test_n_char;

  //Serial.println("nRF52_AT_API::processSerialCharacterAsBleMessage: rx_mode " + String(rx_mode) + ", char = " + String(c) + ", lengthSerialMessage = " + String(lengthSerialMessage()));

  if (rx_mode == RXMODE_LOOK_FOR_COMMAND)
    if (c == EOC) {  //look for the end-of-command character
      processSerialMessage();//the EOC character is NOT added to the serial_buff.  Just go ahead and interpret the serial_buff
    } else {
      addToSerialBuffer(c); //add the character to the buffer

      //look for a space to indicate that a keyword is maybe present
      bool command_is_understood = false;
      if (c == ' ') {
        test_n_char = 3+6+1;   //how long is "BLENOTIFY "
        if (lengthSerialMessage() >= test_n_char) {  //is the current message long enough for this test?
          if (compareStringInSerialBuff("BLENOTIFY ",test_n_char)) {  //does the current message start this way
            ble_command = BLECOMMAND_NOTIFY;
            command_is_understood = true;
          }
        }
        if (command_is_understood == false) {
          test_n_char = 3+5+1;   //how long is "BLEWRITE "
          if (lengthSerialMessage() >= test_n_char) {  //is the current message long enough for this test?
            if (compareStringInSerialBuff("BLEWRITE ",test_n_char)) {  //does the current message start this way
              ble_command = BLECOMMAND_WRITE;
              command_is_understood = true;
            }
          }
        }
        if (command_is_understood == false) {
          //didn't understand the keyword.  switch back to normal UART-like processing
          rx_mode = RXMODE_LOOK_FOR_CR_ONLY;
        } else {
          rx_mode = RXMODE_LOOK_FOR_SERVICE; ble_service_id = 0;
          serial_read_ind = serial_write_ind;  //move up the read pointer
        }
      }
  } else if (rx_mode == RXMODE_LOOK_FOR_SERVICE) {
    if (c == EOC) {  //look for the end-of-command character
      processSerialMessage();//the EOC character is NOT added to the serial_buff.  Just go ahead and interpret the serial_buff
    } else {
      addToSerialBuffer(c); //add the character to the buffer
      if (c == ' ') {
        //interpret the first character as the id
        ble_service_id = (int)(getFirstCharInBuffer() - '0');
        rx_mode = RXMODE_LOOK_FOR_CHARACTERISTIC; ble_char_id = 0;
        serial_read_ind = serial_write_ind;  //clear any remaining message
      }
    }
  } else if (rx_mode == RXMODE_LOOK_FOR_CHARACTERISTIC) {
    if (c == EOC) {  //look for the end-of-command character
      processSerialMessage();//the EOC character is NOT added to the serial_buff.  Just go ahead and interpret the serial_buff
    } else {
      addToSerialBuffer(c); //add the character to the buffer
      if (c == ' ') {
        //interpret the first character as the id
        ble_char_id = (int)(getFirstCharInBuffer() - '0');
        rx_mode = RXMODE_LOOK_FOR_NBYTES; ble_nbytes = 0;
        serial_read_ind = serial_write_ind;  //clear any remaining message
      }
    }
  } else if (rx_mode == RXMODE_LOOK_FOR_NBYTES) {
    if (c == EOC) {  //look for the end-of-command character
      processSerialMessage();//the EOC character is NOT added to the serial_buff.  Just go ahead and interpret the serial_buff
    } else {
      addToSerialBuffer(c); //add the character to the buffer
      if (c == ' ') {
        //interpret the first character as the id
        ble_nbytes = (int)(getFirstCharInBuffer() - '0');
        if ((ble_nbytes > 0) && (ble_nbytes < 10)) {
          //valid!
          rx_mode = RXMODE_LOOK_FOR_DATABYTES;
          serial_read_ind = serial_write_ind;  //clear any remaining message
        } else { 
          //not valid.  switch back to default mode
          rx_mode = RXMODE_LOOK_FOR_CR_ONLY;
        }
      }
    }
  } else if (rx_mode == RXMODE_LOOK_FOR_DATABYTES) {
    if ((lengthSerialMessage() >= ble_nbytes) && (c == EOC)) {
      //message complete!
      int foo_nbytes = min(max_ble_nbytes,ble_nbytes);
      for (int Ibyte = 0; Ibyte < foo_nbytes; Ibyte++) ble_databytes[Ibyte] = (uint8_t)getFirstCharInBuffer();
      sendBleDataByServiceAndChar(ble_command, ble_service_id, ble_char_id, foo_nbytes, ble_databytes);
      serial_read_ind = serial_write_ind;  //clear any remaining message
      rx_mode = RXMODE_LOOK_FOR_ANY;
    } else {
      //still receiving the data bytes
      addToSerialBuffer(c); //add the character to the buffer
    }
  } else {
      //we should hever be here
  }
  return 0;
}

int nRF52_AT_API::lengthSerialMessage(void) {
  int len = 0;
  if (serial_read_ind > serial_write_ind) {
    len = ((nRF52_AT_API_N_BUFFER-1) - serial_read_ind) + serial_write_ind;
  } else {
    len = serial_write_ind - serial_read_ind;
  }
  return len;
}

bool nRF52_AT_API::compareStringInSerialBuff(const char* test_str, int n) {
  int ind = serial_read_ind;  //where to start in the read buffer
  for (int i=0; i < n; i++) {
    if (serial_buff[ind] != test_str[i]) return false;
    ind++;  //increment the read buffer
    if (ind >= nRF52_AT_API_N_BUFFER) ind = 0;  //wrap around the read buffer index, if necessary
  }
  return true;
}

int nRF52_AT_API::processSerialMessage(void) {
  int ret_val = VERB_NOT_KNOWN;
  int len = lengthSerialMessage();     // how long is the message that was received
  int test_n_char = 0;

  //Serial.println("nRF52_AT_API::processSerialMessage: starting...");

  //test for the verb "SEND"
  test_n_char = 4+1;   //how long is "SEND "
  if (len >= test_n_char) {  //is the current message long enough for this test?
    if (compareStringInSerialBuff("SEND ",test_n_char)) {  //does the current message start this way
      serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
      if (DEBUG_VIA_USB) { Serial.print("nRF52_AT_API: recvd: SEND "); debugPrintMsgFromSerialBuff(); Serial.println();}
      bleSendFromSerialBuff(); //must not have any carriage return characters in the payload (other than the trailing carriage return that concludes every message)
      ret_val = 0; 
    }
  }

  //test for the verb "SET"
  test_n_char = 3+1; //how long is "SET "
  if (len >= test_n_char) {
    if (compareStringInSerialBuff("SET ",test_n_char)) {  //does the current message start this way
      serial_read_ind = (serial_read_ind+test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
      if (DEBUG_VIA_USB) { Serial.print("nRF52_AT_API: recvd: SET "); debugPrintMsgFromSerialBuff(); Serial.println(); }
      processSetMessageInSerialBuff();
      ret_val = 0;
    }
  }

  //test for the verb "GET"
  test_n_char = 3+1; //how long is "GET "
  if (len >= test_n_char) {
    if (compareStringInSerialBuff("GET ",test_n_char)) {  //does the current message start this way
      serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
      if (DEBUG_VIA_USB) { Serial.print("nRF52_AT_API: recvd: GET "); debugPrintMsgFromSerialBuff(); Serial.println(); } 
      ret_val = processGetMessageInSerialBuff();
    }
  } 

  //test for the verb "VERSION"
  test_n_char = 7; //how long is "VERSION"
  if (len >= test_n_char) {
    if (compareStringInSerialBuff("VERSION",test_n_char)) {  //does the current message start this way
      serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
      sendSerialOkMessage(versionString);
      ret_val = 0;
    }
  } 


  // serach for another command
  //   anything?

  // give error message if message isn't known
  if (ret_val != 0) {
    if (DEBUG_VIA_USB) {
      if (DEBUG_VIA_USB) { Serial.print("nRF52_AT_API: *** WARNING ***: msg not understood: "); }
      debugPrintMsgFromSerialBuff();
      Serial.println();
    }
  }

  //send FAIL response, if none yet sent
  if (ret_val == VERB_NOT_KNOWN) sendSerialFailMessage("VERB not known");

  //return
  serial_read_ind = serial_write_ind;  //remove any remaining message
  return ret_val;
}

int nRF52_AT_API::processSetMessageInSerialBuff(void) {
  int test_n_char;
  int ret_val = PARAMETER_NOT_KNOWN;

   //look for parameter value of "BEGIN=""
  test_n_char = 5+1; //length of "BAUDRATE="
  if (compareStringInSerialBuff("BEGIN=",test_n_char)) {
    serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
    
    ret_val = setBeginFromSerialBuff();
    if (ret_val == 0) {
      sendSerialOkMessage();
    } else {
      sendSerialFailMessage("SET BEGIN failed");
    }
    serial_read_ind = serial_write_ind;  //remove the message
  }
  
  //look for parameter value of BAUDRATE
  test_n_char = 8+1; //length of "BAUDRATE="
  if (compareStringInSerialBuff("BAUDRATE=",test_n_char)) {
    serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
    //replace this placeholder with useful code
    ret_val = NOT_IMPLEMENTED_YET;
    sendSerialFailMessage("SET BAUDRATE not implemented yet");
    serial_read_ind = serial_write_ind;  //remove the message
  }

  //look for parameter value of MAC
  test_n_char = 3+1; //length of "MAC="
  if (compareStringInSerialBuff("MAC=",test_n_char)) {
    //Serial.println("nRF52_AT_API: processSetMessageInSerialBuff: interpreting MAC");
    serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
    if (bleBegun == true) {
      if (DEBUG_VIA_USB) Serial.println("nRF52_AT_API: processSetMessageInSerialBuff: SET MAC: FAILED: Cannot set MAC after begun");
      sendSerialFailMessage("SET MAC: FAILED: Cannot set MAC after ble has been begun");
    } else {
      if (DEBUG_VIA_USB) {
        Serial.print("nRF52_AT_API: processSetMessageInSerialBuff: setting MAC to ");
        debugPrintMsgFromSerialBuff();
        Serial.println();
      }
      
      ret_val = setMacAddressFromSerialBuff();
      if (ret_val == 0) {
        sendSerialOkMessage();
        //if (DEBUG_VIA_USB) Serial.println("nRF52_AT_API: processSetMessageInSerialBuff: SET MAC: SUCCESS!");
      } else if (ret_val == DATA_WRONG_SIZE) {
        sendSerialFailMessage("SET MAC: FAILED: Given MAC address is wrong size");
        if (DEBUG_VIA_USB) Serial.println("nRF52_AT_API: processSetMessageInSerialBuff: SET MAC: FAILED: Given MAC address is wrong size");
      } else if (ret_val == DATA_WRONG_FORMAT) {
        sendSerialFailMessage("SET MAC: FAILED: Given MAC address was wrong format");
        if (DEBUG_VIA_USB) Serial.println("nRF52_AT_API: processSetMessageInSerialBuff: SET MAC: FAILED: Given MAC address is wrong format");
      } else {
        sendSerialFailMessage("SET MAC: FAILED: (Reason unknown)");
        if (DEBUG_VIA_USB) Serial.println("nRF52_AT_API: processSetMessageInSerialBuff: SET MAC: FAILED: (unknown reason)");
      }
      delay(5);
      for (int i=0; i<24; i++) if (serial_ptr) serial_ptr->print(EOC);    //clear out the UART buffers
    }
    serial_read_ind = serial_write_ind;  //remove the message
  }

  //look for parameter value of NAME
  test_n_char = 4+1; //length of "NAME="
  if (compareStringInSerialBuff("NAME=",test_n_char)) {
    serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
    if (DEBUG_VIA_USB) {
      Serial.print("nRF52_AT_API: processSetMessageInSerialBuff: setting NAME to ");
      debugPrintMsgFromSerialBuff();
      Serial.println();
    }
    
    ret_val = setBleNameFromSerialBuff();
    sendSerialOkMessage();
    delay(5);
    //clear out the UART buffers
    for (int i=0; i<24; i++) {
      if (serial_ptr) serial_ptr->print(EOC);
    }

    serial_read_ind = serial_write_ind;  //remove the message
  }

  //look for parameter value of RFSTATE
  test_n_char = 7+1; //length of "RFSTATE="
  if (compareStringInSerialBuff("RFSTATE=",test_n_char)) {
    serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer

    //replace this placeholder with useful code
    ret_val = NOT_IMPLEMENTED_YET;
    sendSerialFailMessage("SET RFSTATE not implemented yet");

    serial_read_ind = serial_write_ind;  //remove the message
  }

  //look for parameter value of ADVERTISING
  test_n_char = 11+1; //length of "ADVERTISING="
  if (compareStringInSerialBuff("ADVERTISING=",test_n_char)) {
    serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
    ret_val = setAdvertisingFromSerialBuff();
    if (ret_val == 0) {
      sendSerialOkMessage();
    } else {
      sendSerialFailMessage("SET ADVERTISING failed");
    }
    serial_read_ind = serial_write_ind;  //remove the message
  }

  //look for parameter value of LEDMODE
  test_n_char = 7+1; //length of "LEDMODE="
  if (compareStringInSerialBuff("LEDMODE=",test_n_char)) {
    serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
    ret_val = setLedModeFromSerialBuff();
    if (ret_val == 0) {
      sendSerialOkMessage();
    } else {
      sendSerialFailMessage("SET LEDMODE failed");
    }
    serial_read_ind = serial_write_ind;  //remove the message
  }

  // serach for another command
  //   anything?

  // give error message if message isn't known
  if (ret_val != 0) {
    if (DEBUG_VIA_USB) {
      Serial.print("nRF52_AT_API: *** WARNING ***: SET msg not understood: ");
      debugPrintMsgFromSerialBuff();
    }
  }

  //send a FAIL message if none has been sent yet
  if (ret_val == PARAMETER_NOT_KNOWN) sendSerialFailMessage("SET parameter not known");

  serial_read_ind = serial_write_ind;  //remove the message
  return ret_val;
}

int nRF52_AT_API::processGetMessageInSerialBuff(void) {
  int test_n_char;
  int ret_val = PARAMETER_NOT_KNOWN;
  
  //Serial.print("nRF52_AT_API::processGetMessageInSerialBuff: interpreting ");  debugPrintMsgFromSerialBuff(); Serial.println();
  //Serial.println("nRF52_AT_API::processGetMessageInSerialBuff: compareStringInSerialBuff('NAME',test_n_char) = " + String(compareStringInSerialBuff("NAME",test_n_char)));

  test_n_char = 8; //length of "BAUDRATE"
  if (compareStringInSerialBuff("BAUDRATE",test_n_char)) {
    int foo_ind = serial_read_ind+test_n_char;
    while (foo_ind >= nRF52_AT_API_N_BUFFER) foo_ind -= nRF52_AT_API_N_BUFFER;
    if ((foo_ind==serial_write_ind) || (serial_buff[foo_ind] == EOC)) {
      ret_val = NOT_IMPLEMENTED_YET;
      sendSerialFailMessage("GET BAUDRATE not implemented yet");
    } else {
      ret_val = FORMAT_PROBLEM;
      sendSerialFailMessage("GET BAUDRATE had formatting problem");
    }     
    serial_read_ind = serial_write_ind;  //remove the message
  }

  test_n_char = 4; //length of "NAME"
  if (compareStringInSerialBuff("NAME",test_n_char)) {
    int foo_ind = serial_read_ind+test_n_char;
    while (foo_ind >= nRF52_AT_API_N_BUFFER) foo_ind -= nRF52_AT_API_N_BUFFER;
    if ((foo_ind==serial_write_ind) || (serial_buff[foo_ind] == EOC)) {
        //get the current BLE name 
        static const uint16_t n_len = 64;
        char name[n_len];
        uint32_t act_len = Bluefruit.getName(name, n_len);
        name[act_len]='\0';  //the BLE module does not appear to null terminate, so let's do it ourselves
        if ((act_len > 0) || (act_len < n_len)) {
          ret_val = 0;  //it's good!
          sendSerialOkMessage(name); //send via the BLE link
        } else {
          ret_val = 3;
          sendSerialFailMessage("GET NAME Could not get NAME from nRF module");
        }
    } else {
      ret_val = 2;
      sendSerialFailMessage("GET NAME had formatting problem");
      serial_read_ind = serial_write_ind;  //remove the message
    }
  } 

  test_n_char = 7; //length of "RFSTATE"
  if (compareStringInSerialBuff("RFSTATE",test_n_char)) {
    int foo_ind = serial_read_ind+test_n_char;
    while (foo_ind >= nRF52_AT_API_N_BUFFER) foo_ind -= nRF52_AT_API_N_BUFFER;
    if ((foo_ind==serial_write_ind) || (serial_buff[foo_ind] == EOC)) {
      ret_val = NOT_IMPLEMENTED_YET;
      sendSerialFailMessage("GET RFSTATE not implemented yet");
    } else {
      ret_val = FORMAT_PROBLEM;
      sendSerialFailMessage("GET RFSTATE had formatting problem");
    }     
    serial_read_ind = serial_write_ind;  //remove the message
  }  

  test_n_char = 11; //length of "ADVERTISING"
  if (compareStringInSerialBuff("ADVERTISING",test_n_char)) {
    int foo_ind = serial_read_ind+test_n_char;
    while (foo_ind >= nRF52_AT_API_N_BUFFER) foo_ind -= nRF52_AT_API_N_BUFFER;
    if ((foo_ind==serial_write_ind) || (serial_buff[foo_ind] == EOC)) {
      ret_val = 0;
      sendSerialOkMessage((Bluefruit.Advertising.isRunning()) ? "TRUE" : "FALSE");
    } else {
      ret_val = FORMAT_PROBLEM;
      sendSerialFailMessage("GET ADVERTISING had formatting problem");
    }     
    serial_read_ind = serial_write_ind;  //remove the message
  } 

  test_n_char = 7; //length of "LEDMODE"
  if (compareStringInSerialBuff("LEDMODE",test_n_char)) {
    int foo_ind = serial_read_ind+test_n_char;
    while (foo_ind >= nRF52_AT_API_N_BUFFER) foo_ind -= nRF52_AT_API_N_BUFFER;
    if ((foo_ind==serial_write_ind) || (serial_buff[foo_ind] == EOC)) {
      ret_val = 0;
      if (led_control.ledToFade==0) {
        sendSerialOkMessage("0");
      } else {
        sendSerialOkMessage("1");
      }
    } else {
      ret_val = FORMAT_PROBLEM;
      sendSerialFailMessage("GET LEDMODE had formatting problem");
    }     
    serial_read_ind = serial_write_ind;  //remove the message
  }  

  test_n_char = 9; //length of "CONNECTED"
  if (compareStringInSerialBuff("CONNECTED",test_n_char)) {
    int foo_ind = serial_read_ind+test_n_char;
    while (foo_ind >= nRF52_AT_API_N_BUFFER) foo_ind -= nRF52_AT_API_N_BUFFER;
    if ((foo_ind==serial_write_ind) || (serial_buff[foo_ind] == EOC)) {
      ret_val = 0;
      sendSerialOkMessage((Bluefruit.connected()) ? "TRUE" : "FALSE");
    } else {
      ret_val = FORMAT_PROBLEM;
      sendSerialFailMessage("GET CONNECTED had formatting problem");
    }     
    serial_read_ind = serial_write_ind;  //remove the message
  }  

  test_n_char = 7; //length of "VERSION"
  if (compareStringInSerialBuff("VERSION",test_n_char)) {
    int foo_ind = serial_read_ind+test_n_char;
    while (foo_ind >= nRF52_AT_API_N_BUFFER) foo_ind -= nRF52_AT_API_N_BUFFER;
    if ((foo_ind==serial_write_ind) || (serial_buff[foo_ind] == EOC)) {
      ret_val = 0;
      sendSerialOkMessage(versionString);
    } else {
      ret_val = FORMAT_PROBLEM;
      sendSerialFailMessage("GET VERSION had formatting problem");
    }     
    serial_read_ind = serial_write_ind;  //remove the message
  }  

  // serach for another command
  //   anything?
  if (ret_val == PARAMETER_NOT_KNOWN) {
    sendSerialFailMessage("GET parameter not known");
  }

  // give error message if message isn't known
  if (ret_val != 0) {
    //Serial.print("nRF52_AT_API: *** WARNING ***: GET msg not understood: ");
    //debugPrintMsgFromSerialBuff();
    serial_read_ind = serial_write_ind;  //remove the message
  }

  return ret_val;
}

//returns OPERATION_FAILED if failed
int nRF52_AT_API::setBeginFromSerialBuff(void) {
  int ret_val = OPERATION_FAILED;
  int read_ind = serial_read_ind;
  if (serial_buff[serial_read_ind] == '0') { //for FALSE
    //do not start
    ret_val = 0;
  } else {
    //any other character (including none), assume we want to start
    int id = (int) (serial_buff[serial_read_ind] - '0');  //cheating, assumes only 1 character
    beginAllBleServices(id);
    ret_val = 0;
  }
  serial_read_ind = serial_write_ind;  //remove any remaining message
  return ret_val;
}

//returns
int nRF52_AT_API::setMacAddressFromSerialBuff(void) {
  int ret_val = 0;
  if (bleBegun == true)  {
    if (DEBUG_VIA_USB) Serial.println("nRF52_AT_API: setMacAddressFromSerialBuff: ERROR: cannot set name after ble.begin()");
  } else {
    int end_ind = serial_read_ind;

    //read the name...6 hexadecimal values
    static const int mac_len = 2*6;  //6 hexadecimal values 
    char new_mac[mac_len+1]; //11 characters plus the null termination
    //if (DEBUG_VIA_USB) Serial.println("nRF52_AT_API: setMacAddressFromSerialBuff: lengthSerialMessage() =" + String(lengthSerialMessage()));
    if (lengthSerialMessage() < mac_len) {
      //too short!
      serial_read_ind = serial_write_ind;  //remove the message
      ret_val = DATA_WRONG_SIZE;
    } else {
      //length is long enough, so let's continue interpretting the String
      bool done = false;
      int targ_ind = 0;
      while (!done) {
        char c = serial_buff[serial_read_ind++];
        if ((c >= 0) && (c <= 9) || ((c >= 'A') && (c <= 'F')) || ((c >= 'a') && (c <= 'f'))) {
          //good
          new_mac[targ_ind++] = c;
        } else {
          //bad
          ret_val = DATA_WRONG_FORMAT;
          done=true;
        }
        if (targ_ind >= mac_len) done = true; 
      }
      setMacAddress(new_mac);
    }
  }
  serial_read_ind = serial_write_ind;  //remove any remaining message
  return ret_val;
}

//returns OPERATION_FAILED if failed
int nRF52_AT_API::setBleNameFromSerialBuff(void) {
  int end_ind = serial_read_ind;
  if (serial_buff[serial_read_ind] == EOC) {
    //no name was given.  do nothing
  } else {
    //copy the new name, up to max characters, or until we hit EOC
    static const int max_len_name = 16;  //16 character max?? 
    char new_name[max_len_name+1]; //11 characters plus the null termination
    bool done = false;
    int targ_ind = 0;
    while (!done) {
      new_name[targ_ind++] = serial_buff[serial_read_ind++];
      while (serial_read_ind >= nRF52_AT_API_N_BUFFER) serial_read_ind -= nRF52_AT_API_N_BUFFER;
      if (targ_ind >= max_len_name) done = true;
      if (serial_buff[serial_read_ind] == EOC) done = true;
      if (serial_read_ind == serial_write_ind) done = true;
    }

    //finish off the new name
    if (targ_ind < max_len_name) {
      for (int i=targ_ind; i < max_len_name+1; i++ ) new_name[i] = '\0';  //add the null termination
    }
    if (DEBUG_VIA_USB) Serial.println("nRF52_AT_API: setBleNameFromSerialBuff: new_name = " + String(new_name));

    //prepare to set the new name...stop any advertising
    Bluefruit.Advertising.stop();
    Bluefruit.Advertising.clearData();
    Bluefruit.ScanResponse.clearData(); // add this

    //send the new name to the module
    Bluefruit.setName(new_name);

    //restart advertising
    startAdv();

    return 0;  //return OK
  }

  return OPERATION_FAILED;
}

int nRF52_AT_API::setAdvertisingFromSerialBuff(void) {
  int ret_val = OPERATION_FAILED;
  int read_ind = serial_read_ind;
  if ((lengthSerialMessage() > 0) && (serial_buff[read_ind]==' ')) read_ind++; //remove leading whitespace
  if (lengthSerialMessage() >= 2) {
    int next_read_ind = read_ind+1;
    while (next_read_ind >= nRF52_AT_API_N_BUFFER) next_read_ind -= nRF52_AT_API_N_BUFFER;
    if ((serial_buff[read_ind]=='O') && (serial_buff[next_read_ind]=='N')) {  //look for ON
      startAdv();
      ret_val = 0;
    } else if ((serial_buff[read_ind]=='O') && (serial_buff[next_read_ind]=='F')) {  //look for OFF
      stopAdv();
      ret_val = 0;
    }
  }
  return ret_val;
}


int nRF52_AT_API::setLedModeFromSerialBuff(void) {
  int ret_val = OPERATION_FAILED;
  int read_ind = serial_read_ind;
  if ((lengthSerialMessage() > 0) && (serial_buff[read_ind]==' ')) read_ind++; //remove leading whitespace
  while (read_ind >= nRF52_AT_API_N_BUFFER) read_ind -= nRF52_AT_API_N_BUFFER;
  if (lengthSerialMessage() >= 1) {
    if (serial_buff[read_ind]=='0') {
      led_control.disableLEDs();
      ret_val = 0;
    } else if (serial_buff[read_ind]=='1') {
      led_control.setLedColor(led_control.red);
      ret_val = 0;
    }
  }
  return ret_val;
}

//Send is for text-like data payloads to be sent via UART.  Cannot have a carriage return in the data payload.
//Must still have a carriage return at the end of the serial buffer, though, marking the end of the overall message
int nRF52_AT_API::bleSendFromSerialBuff(void) {
  //copy the message from the circular buffer to the straight buffer
  int counter = 0;
  while (serial_read_ind != serial_write_ind) {
    BLEmessage[counter] = serial_buff[serial_read_ind];
    counter++;
    serial_read_ind++;  
    if (serial_read_ind >= nRF52_AT_API_N_BUFFER) serial_read_ind = 0;
  }

  //if BLE is connected, fire off the message
  if (bleConnected) {
    if (ble_ptr1) ble_ptr1->write( BLEmessage, counter );
    if (ble_ptr2) ble_ptr2->write( BLEmessage, counter );
    return counter;
  }
  return NO_BLE_CONNECTION;
}

void nRF52_AT_API::sendSerialOkMessage(const char* reply_str) {
  serial_ptr->print("OK ");
  serial_ptr->print(reply_str);
  serial_ptr->println(EOC);
}
void nRF52_AT_API::sendSerialOkMessage(void) {
  serial_ptr->print("OK ");
  serial_ptr->println(EOC);
}
void nRF52_AT_API::sendSerialFailMessage(const char* reply_str) {
  serial_ptr->print("FAIL ");
  serial_ptr->print(reply_str);
  serial_ptr->println(EOC);
}

void nRF52_AT_API::debugPrintMsgFromSerialBuff(void) {
  debugPrintMsgFromSerialBuff(serial_read_ind, serial_write_ind);
}

void nRF52_AT_API::debugPrintMsgFromSerialBuff(int start_ind, int end_ind) {
  while (start_ind != end_ind) {
    if (DEBUG_VIA_USB) Serial.print(serial_buff[start_ind]);
    start_ind++; 
    if (start_ind >= nRF52_AT_API_N_BUFFER) start_ind = 0;
  }  
}



#endif
