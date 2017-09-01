/* 
 * Wrapper file to connect all things with each other.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "sub_reader.h"
#include "av_decoder.h"
#include "config.h"
#include "lsh_db.h"

int main(int argc,char *argv[])
{
  
  char *subtitle_filename,*video_filename,*edited_filename;
  //Note: edited_filename can be either audio/video file of edited video
  int ret,i,j,index;
  int64_t delay,start_pts,duration,num,den,granuality;
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
  ret = reader(subtitle_filename);
  if(ret != 0)
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: subtitle reader returned %d\n"ANSI_COLOR_RESET,ret);

  set_current_processing_video(1);
  ret = get_video_info(1,&num,&den,&start_pts,&duration);
  if(ret != 0){
    fprintf(stderr,ANSI_COLOR_ERROR"ERROR: Unable to get video information for edited file\n");
    exit(0);
  }
  fprintf(stdout,ANSI_COLOR_DEBUG"DEBUG: start_pts %lu timebase %lu/%lu\n",start_pts,num,den);
  granuality = (GRANUALITY/1000) * den/num;  
  do{
    index = 0;
    ret = create_fingerprint_by_pts(index,start_pts);
    if(ret != 0)
      fprintf(stderr,ANSI_COLOR_ERROR"ERROR: create_fingerprint returned %d\n"ANSI_COLOR_RESET,ret);
    start_pts = start_pts + granuality;
    index++;
  } while(ret == 0);
  fprintf(stdout,"DEBUG: Proposed timing 2.5 sec pts-Delay %lld PTS units\n",delay);
 
  //  ret = reader(subtitle_filename);
  close_db();
  close_filter();
  if(ret != 0){
    fprintf(stderr,"ERROR: Unable to open file %s\n",subtitle_filename);
    return -1;
  }
}

