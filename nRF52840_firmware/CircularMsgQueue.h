
#pragma once

#define TOTAL_BYTES_TO_USE (150000)       // total RAM to use for the combination of the queue array and for the message_end_inds array
#define TYPICAL_PAKCET_SIZE_BYTES 24      // typical length of a data packet from the Tympan to convey out via BLE
#define STORAGE_BYTES_REQ_PER_PACKET (TYPICAL_PAKCET_SIZE_BYTES + (1*4))    //the packet payload plus an entry in the message_ind_inds array
#define MAX_MSGS_QUEUED ((int)(TOTAL_BYTES_TO_USE/STORAGE_BYTES_REQ_PER_PACKET))
#define QUEUE_LEN_BYTES (MAX_MSGS_QUEUED * TYPICAL_PAKCET_SIZE_BYTES)

class CircularMessageQueue { 
  public:
    void reset(void);

    int enqueue(uint8_t *data_in, int len);  //return non-zero if error
    int dequeue(uint8_t *data_out, int *len, int max_len);   //return non-zero if error

    int getNumMsgs(void) const;
    int getNumFreeMsgs(void) const {  return MAX_MSGS_QUEUED - getNumMsgs(); }
    int getNumBytesInQueue(void) const;
    int getNumFreeBytesInQueue(void) const { return QUEUE_LEN_BYTES - getNumBytesInQueue();}

    void incrementToReadNextMsg(void);

  private:
    //underlyng circular buffer for the bytes
    int buffer_write = 0, buffer_read = 0;
    uint8_t buffer[QUEUE_LEN_BYTES];
    
    //circular buffer for the start/end of each message
    int msg_write = 0, msg_read = 0;
    int message_end_inds[MAX_MSGS_QUEUED];
  

    //private methods
    void addMsgEndIndex(const int end_ind) {  //this will wrap around without warning!  so, check yourself first!
      message_end_inds[msg_write] = end_ind;
      msg_write = (msg_write + 1) % MAX_MSGS_QUEUED;
    }

    int getNextMsgEnd(int *end_ind) const { //return non-zero if error
      if (getNumMsgs() <= 0) return -1;
      *end_ind = message_end_inds[msg_read];
      //msg_read = (msg_read + 1) % MAX_MSGS_QUEUED;
      return 0;
    }


};

void CircularMessageQueue::reset(void) {
  //reset the buffer of bytes
  buffer_write = 0;  buffer_read=0;
  //reset the buffer of message locations
  msg_write=0;  msg_read=0;
} 

int CircularMessageQueue::enqueue(uint8_t *data_in, int len) { //return non-zero if error
  //check to make sure that we have space
  int nFreeBytesInQueue = getNumFreeBytesInQueue();
  if (len > nFreeBytesInQueue) return -1;  //not enough space!
  int nFreeMsgs = getNumFreeMsgs();
  if (nFreeMsgs <= 0) return -2;  //not enough space!

  //copy the bytes into the byte queue
  int Idest = buffer_write;
  for (int Isource = 0; Isource < len; Isource++) {
    buffer[Idest] = data_in[Isource];
    Idest++;
    if (Idest >= QUEUE_LEN_BYTES) Idest = 0;
  }

  //save this location as the end of this message
  buffer_write = Idest;
  addMsgEndIndex(buffer_write);
  return 0;  //success!
} 

int CircularMessageQueue::dequeue(uint8_t *data_out, int *len, int max_len) {  //return non-zero if error
  //get indices for the next message
  int msg_start = buffer_read;
  int msg_end = 0;
  int error_val = getNextMsgEnd(&msg_end);
  if (error_val) return error_val;

  //make sure that we have room in given array to put our bytes
  int len_msg = msg_end - msg_start;
  if (len_msg < 0) len_msg = QUEUE_LEN_BYTES - msg_end + msg_start;
  if (len_msg > max_len) return -2; //message is too big for the given pointer!

  //copy our queued bytes out to the given pointer
  int Isource = msg_start;
  for (int Idest = 0; Idest < len_msg; Idest++) {
    data_out[Idest] = buffer[Isource];
    Isource++;  if (Isource >= QUEUE_LEN_BYTES) Isource = 0;
  }
  *len = len_msg; //copy the message length to the return value

  //we've copied the data, so we can now increment our indices and message counter for the next time
  buffer_read = msg_end;     //this should be the start of the next message 
  incrementToReadNextMsg();   //this should increment to point us to the end of the next message
  return 0;  //success!
}

int CircularMessageQueue::getNumMsgs(void) const { 
  if (msg_write > msg_read) {
    return msg_write - msg_read;
  } else if (msg_read > msg_write) {
    return (MAX_MSGS_QUEUED - msg_read + msg_write);
  }
  return 0;
}


int CircularMessageQueue::getNumBytesInQueue(void) const {
  if (buffer_write > buffer_read) {
    return buffer_write - buffer_read;
  } else if (buffer_read > buffer_write) {
    return (QUEUE_LEN_BYTES - buffer_read + buffer_write);
  }
  return 0;  //buffer_write and buffer_read are the same
}

void CircularMessageQueue::incrementToReadNextMsg(void) {
  if (getNumMsgs() <= 0) return;  //there are no next messages
  
  //force the buffer read index for the new message
  buffer_read = message_end_inds[msg_read];
  
  //increment the message index
  msg_read = (msg_read + 1) % MAX_MSGS_QUEUED;
}
