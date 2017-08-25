#ifndef SUB_READER
#define SUB_READER

#endif

#include <stdint.h>
#include <inttypes.h>
#include "config.h"

// block for one 1.5 second granuality. Note: 
struct subtitle_block{
  uint16_t index; //Index of subtitle in file
  LLONG start_pts,end_pts; // time from which fingerprint will start till 1.5 sec
  char *sub_string;
};

int reader(const char *);
LLONG pts_to_mseconds(char *);
void add_timestamp_to_array(uint16_t,char *);  
