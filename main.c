/* 
 * Wrapper file to connect all things with each other.
 */
#include <stdio.h>
#include <unistd.h>
#include "sub_reader.h"
#include "av_decoder.h"
#include "config.h"
#include "lsh_db.h"

int main(int argc,char *argv[])
{
  
  char *subtitle_filename,*video_filename,*edited_filename;
  //Note: edited_filename can be either audio/video file of edited video
  int ret,i,j;
  fprintf(stderr,"Number of args:%d\n",argc);
  if(argc != 4){
    fprintf(stderr,"ERROR: Requires atleast 2 arguements \nUsage: audiof <subtitle-filename> <orig-video-filename> <edited-video-filename\n");
    exit(2); //exit status for command line error
  }
  else if(argc == 4){
    //subtitle filename requires fnmatch
    subtitle_filename = argv[1];
    video_filename = argv[2];
    edited_filename = argv[3];

    if(!(access(subtitle_filename,R_OK) != -1)){
      fprintf(stderr,"Unable to access file %s\n",subtitle_filename);
      exit(1);
    }
    if(init_decoder(video_filename,edited_filename) != 0){
      fprintf(stderr,"init_decoder failed for file\n");
      exit(0);
    }
  }
  else {
    fprintf(stderr,"ERROR: Something else\n");
    exit(0);
  }

  set_current_processing_video(0); //original video
  init_db();
  create_fingerprint_by_pts(0,0);

  set_current_processing_video(1);
  create_fingerprint_by_pts(0,0);

  //  ret = reader(subtitle_filename);
  close_db();
  close_filter();
  if(ret != 0){
    fprintf(stderr,"ERROR: Unable to open file %s\n",subtitle_filename);
    return -1;
  }
}

