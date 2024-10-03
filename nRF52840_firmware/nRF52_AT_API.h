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
extern bool bleConnected;
extern char BLEmessage[];
extern void startAdv(void);
extern void stopAdv(void);
extern const char versionString[];

//running on the nRF52, this interprets commands coming from the hardware serial and send replies out to the BLE
#define nRF52_AT_API_N_BUFFER 512
class nRF52_AT_API {
  public:
    nRF52_AT_API(BLEUart_Tympan *_bleuart1, HardwareSerial *_ser_ptr) : ble_ptr1(_bleuart1) , serial_ptr(_ser_ptr) {}
    nRF52_AT_API(BLEUart_Tympan *_bleuart1, BLEUart *_bleuart2, HardwareSerial *_ser_ptr) : ble_ptr1(_bleuart1), ble_ptr2(_bleuart2), serial_ptr(_ser_ptr) {}
    
    virtual int processSerialCharacter(char c);  //here's the main entry point to the AT message parsing
    virtual int lengthSerialMessage(void);
    virtual int processSerialMessage(void);

  protected:
    BLEUart_Tympan *ble_ptr1 = NULL;
    BLEUart *ble_ptr2 = NULL;
    HardwareSerial *serial_ptr = &Serial1;
    char EOC = '\r'; //all commands (including "SEND") from the Tympan must end in this character

    //circular buffer for reading from Serial
    char serial_buff[nRF52_AT_API_N_BUFFER];
    int serial_read_ind = 0;
    int serial_write_ind = 0;

    bool compareStringInSerialBuff(const char* test_str, int n);  
    int processSetMessageInSerialBuff(void);  
    int processGetMessageInSerialBuff(void);
    int setBleNameFromSerialBuff(void);
    int setAdvertisingFromSerialBuff(void);
    int setLedModeFromSerialBuff(void);
    int bleWriteFromSerialBuff(void);
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
    const int NO_BLE_CONNECTION = 99;
};


//here's the main entry point to the AT message parsing
int nRF52_AT_API::processSerialCharacter(char c) {
  //look for carriage return
  if (c == EOC) {  //look for the end-of-command character
    //the EOC character is NOT added to the serial_buff.  Just go ahead and interpret the serial_buff
    processSerialMessage();
  } else {
    serial_buff[serial_write_ind] = c;
    serial_write_ind++;
    if (serial_write_ind >= nRF52_AT_API_N_BUFFER) serial_write_ind = 0;
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

  //Serial.println("nRF52_AT_API::processSerialMessage: starting...");

  //test for the verb "SEND"
  int test_n_char = 5;   //how long is "SEND "
  if (len >= test_n_char) {  //is the current message long enough for this test?
    if (compareStringInSerialBuff("SEND ",test_n_char)) {  //does the current message start this way
      serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
      if (DEBUG_VIA_USB) { Serial.print("nRF52_AT_API: recvd: SEND "); debugPrintMsgFromSerialBuff(); Serial.println();}
      bleWriteFromSerialBuff();
      ret_val = 0; 
    }
  }

  //test for the verb "SET"
  test_n_char = 4; //how long is "SET "
  if (len >= test_n_char) {
    if (compareStringInSerialBuff("SET ",test_n_char)) {  //does the current message start this way
      serial_read_ind = (serial_read_ind+test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer
      if (DEBUG_VIA_USB) { Serial.print("nRF52_AT_API: recvd: SET "); debugPrintMsgFromSerialBuff(); Serial.println(); }
      processSetMessageInSerialBuff();
      ret_val = 0;
    }
  }

  //test for the verb "GET"
  test_n_char = 4; //how long is "GET "
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
  
  //look for parameter value of BAUDRATE
  test_n_char = 8+1; //length of "BAUDRATE="
  if (compareStringInSerialBuff("BAUDRATE=",test_n_char)) {
    serial_read_ind = (serial_read_ind + test_n_char) % nRF52_AT_API_N_BUFFER; //increment the reader index for the serial buffer

    //replace this placeholder with useful code
    ret_val = NOT_IMPLEMENTED_YET;
    sendSerialFailMessage("SET BAUDRATE not implemented yet");

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
    
    #if 1
      ret_val = setBleNameFromSerialBuff();
      sendSerialOkMessage();
      delay(5);
      //clear out the UART buffers
      for (int i=0; i<24; i++) {
        if (serial_ptr) serial_ptr->print(EOC);
      }
    #else
      ret_val = NOT_IMPLEMENTED_YET;
      sendSerialFailMessage("SET NAME not implemented yet");
    #endif
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


int nRF52_AT_API::bleWriteFromSerialBuff(void) {
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
