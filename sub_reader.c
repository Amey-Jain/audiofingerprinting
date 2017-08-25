/*
Reads subtitles from input file, and returns timing of those subtitles back.
Currently it can read .srt files, more formats to be added later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <wchar.h>
#include <locale.h>
#include <ctype.h>
#include <errno.h>
#include "sub_reader.h"
#include "config.h"
#include "av_decoder.h"

LLONG pts_to_mseconds(char *str) // converts timestamp in *str to LLONG in seconds
{
 LLONG pts=0;
 uint16_t hh,mm,ss,ms;
 if(str != NULL && strlen(str) != 12)
   return -1;
 hh = atoi(&str[0]);
 mm = atoi(&str[3]);
 ss = atoi(&str[6]);
 ms = atoi(&str[9]);
 pts = (((hh * 3600) + (mm * 60) + ss) * 1000) + ms;
 return pts;
}

void add_timestamp_to_array(uint16_t index,char *time_stamp)
{
  char *temp; 
  LLONG show_pts_ms=0,hide_pts_ms=0,temp_pts_ms=0;
  uint8_t no_of_fp = 0,i; // number of fingerprint(1.5 sec) in subtitle block
  temp = malloc(sizeof(uint8_t) * 13);
  if(temp == NULL)
    return;
  printf("%s received time stamp %s\n",__FUNCTION__,time_stamp);
  temp = strncpy(temp,time_stamp,12);
  temp[12] = '\0';
  show_pts_ms = pts_to_mseconds(temp);
  temp = strncpy(temp,&time_stamp[17],12);
  hide_pts_ms = pts_to_mseconds(temp);
  free(temp);
 
  //This calculation may miss some of points of fingerprinting
  no_of_fp = (hide_pts_ms - show_pts_ms) / GRANUALITY;
  temp_pts_ms = show_pts_ms;
  i=0;
  do{
    //send temp_pts to next function
    create_fingerprint_by_pts(index,temp_pts_ms); 
    fprintf(stderr,"DEBUG:PTS: %u\n",temp_pts_ms);
    temp_pts_ms = temp_pts_ms + GRANUALITY;
    i++;
  }while(i<no_of_fp);
}


int reader(const char *file_name)
{
  FILE *input_handle;
  char *buf = NULL;
  uint8_t i;
  uint8_t t[3];
  uint16_t temp;
  uint16_t index_buf = 0;
  uint8_t line_ending;
  struct subtitle_block current;
  input_handle = fopen(file_name,"rb");
  if(input_handle == NULL){
    fprintf(stderr,"Unable to open file: %s Error %d\n",file_name,errno);
    return -1;
  }
  else{
    fprintf(stdout,"Opening file: %s\n",file_name);
    buf = malloc(sizeof(uint8_t) * 4);
    buf = fgets(buf,sizeof(uint8_t) * 4,input_handle);
    for(i=0;i<3;i++)
      t[i] = buf[i];
    if(t[0]==0xef && t[1]==0xbb && t[2]==0xbf){
      //file is utf-8 type
      // main loop for reading subtitle timings
      printf("UTF-8 detected\n");
    }
    else if(t[0] == '1' && (t[1] =='\r' && t[2] == '\n') || t[1] == '\n'){
      current.index = 1;
    }
    //TODO handling of other than UTF-8
    else{
      fprintf(stderr,"File other than UTF-8 not supported yet\nGot this buffer from file %s\n",buf);
      free(buf);
      return -1;
    }
    free(buf);
    
    buf = malloc(sizeof(uint8_t) * BUF_SIZE);
    while(1){//main loop
      buf = fgets(buf,sizeof(uint8_t) * BUF_SIZE,input_handle);
      i = 0;
      if(buf == NULL){
	break;
      }
      else{
	if((temp = strlen(buf)) >= 3 && isdigit(buf[i]) && buf[temp - 2]=='\r' && buf[temp - 1]=='\n'){
	  index_buf = (index_buf ?  :atoi(&buf[i]));
	  current.index = index_buf;
	}
	if((strlen(buf) == 29) && index_buf){
	  add_timestamp_to_array(index_buf,buf);
	  index_buf = 0;	
	}
      }
    }
    free(buf); 
  }
  fclose(input_handle);
  return 0;
}

