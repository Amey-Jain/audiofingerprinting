/* 
 * Wrapper file to connect all things with each other.
 */
#include <stdio.h>
#include "sub_reader.h"
#include "av_decoder.h"
#include "config.h"

int main(int argc,char *argv[])
{
  char *subtitle_filename,*video_filename1,*video_filename2;
  int ret;
  fprintf(stderr,"Number of args:%d\n",argc);
  if(argc > 4){
    fprintf(stderr,"ERROR: Requires atleast 2 arguements \nUsage: audiof <subtitle-filename> <orig-video-filename> <edited-video-filename\n");
    exit(0);
  }
  else if(argc == 3){
    subtitle_filename = argv[1];
    video_filename1 = argv[2];
    if(init_decoder(video_filename1,NULL,1) != 1){
      fprintf(stderr,"init_decoder failed\n");
      exit(0);
    }
  }
  else {
    fprintf(stderr,"Flags not supported yet\n");
    exit(0);
  }

  ret = reader(subtitle_filename);
  close_filter();
  if(ret != 0){
    fprintf(stderr,"ERROR: Unable to open file %s\n",subtitle_filename);
    return -1;
  }
}

